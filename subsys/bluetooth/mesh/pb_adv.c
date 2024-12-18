/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <string.h>
#include <bluetooth/conn.h>
#include <bluetooth/mesh.h>
#include <net/buf.h>
#include "host/testing.h"
#include "net.h"
#include "adv.h"
#include "crypto.h"
#include "beacon.h"
#include "host/ecc.h"
#include "prov.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_PROV)
#define LOG_MODULE_NAME bt_mesh_pb_adv
#include "common/log.h"

#define GPCF(gpc)           (gpc & 0x03)
#define GPC_START(last_seg) (((last_seg) << 2) | 0x00)
#define GPC_ACK             0x01
#define GPC_CONT(seg_id)    (((seg_id) << 2) | 0x02)
#define GPC_CTL(op)         (((op) << 2) | 0x03)

#define START_PAYLOAD_MAX 20
#define CONT_PAYLOAD_MAX  23

#define START_LAST_SEG(gpc) (gpc >> 2)
#define CONT_SEG_INDEX(gpc) (gpc >> 2)

#define BEARER_CTL(gpc) (gpc >> 2)
#define LINK_OPEN       0x00
#define LINK_ACK        0x01
#define LINK_CLOSE      0x02

#define XACT_SEG_DATA(_seg) (&pb_link.rx.buf->data[20 + ((_seg - 1) * 23)])
#define XACT_SEG_RECV(_seg) (pb_link.rx.seg &= ~(1 << (_seg)))

#define XACT_ID_MAX  0x7f
#define XACT_ID_NVAL 0xff
#define SEG_NVAL     0xff

#define RETRANSMIT_TIMEOUT  K_MSEC(CONFIG_BT_MESH_PB_ADV_RETRANS_TIMEOUT)
#define BUF_TIMEOUT         K_MSEC(400)
#define CLOSING_TIMEOUT     (3 * MSEC_PER_SEC)
#define TRANSACTION_TIMEOUT (30 * MSEC_PER_SEC)

/* Acked messages, will do retransmissions manually, taking acks into account:
 */
#define RETRANSMITS_RELIABLE   0
/* PDU acks: */
#define RETRANSMITS_ACK        2
/* Link close retransmits: */
#define RETRANSMITS_LINK_CLOSE 2

enum {
	ADV_LINK_ACTIVE,    	/* Link has been opened */
	ADV_LINK_ACK_RECVD, 	/* Ack for link has been received */
	ADV_LINK_CLOSING,   	/* Link is closing down */
	ADV_LINK_INVALID,   	/* Error occurred during provisioning */
	ADV_ACK_PENDING,    	/* An acknowledgment is being sent */
	ADV_PROVISIONER,    	/* The link was opened as provisioner */

	ADV_NUM_FLAGS,
};

struct pb_adv {
	uint32_t id; /* Link ID */

	ATOMIC_DEFINE(flags, ADV_NUM_FLAGS);

	const struct prov_bearer_cb *cb;
	void *cb_data;

	struct {
		uint8_t id;       /* Most recent transaction ID */
		uint8_t seg;      /* Bit-field of unreceived segments */
		uint8_t last_seg; /* Last segment (to check length) */
		uint8_t fcs;      /* Expected FCS value */
		struct net_buf_simple *buf;
	} rx;

	struct {
		/* Start timestamp of the transaction */
		int64_t start;

		/* Transaction id */
		uint8_t id;

		/* Current ack id */
		uint8_t pending_ack;

		/* Pending outgoing buffer(s) */
		struct net_buf *buf[3];

		prov_bearer_send_complete_t cb;

		void *cb_data;

		/* Retransmit timer */
		struct k_work_delayable retransmit;
	} tx;

	/* Protocol timeout */
	struct k_work_delayable prot_timer;
};

struct prov_rx {
	uint32_t link_id;
	uint8_t xact_id;
	uint8_t gpc;
};

NET_BUF_SIMPLE_DEFINE_STATIC(rx_buf, 65);

static struct pb_adv pb_link = { .rx = { .buf = &rx_buf } };

static void gen_prov_ack_send(uint8_t xact_id);
static void link_open(struct prov_rx *rx, struct net_buf_simple *buf);
static void link_ack(struct prov_rx *rx, struct net_buf_simple *buf);
static void link_close(struct prov_rx *rx, struct net_buf_simple *buf);
static void prov_link_close(enum prov_bearer_link_status status);
static void close_link(enum prov_bearer_link_status status);

static void buf_sent(int err, void *user_data)
{
	if (atomic_test_and_clear_bit(pb_link.flags, ADV_LINK_CLOSING)) {
		close_link(PROV_BEARER_LINK_STATUS_SUCCESS);
		return;
	}
}

static void buf_start(uint16_t duration, int err, void *user_data)
{
	if (err) {
		buf_sent(err, user_data);
	}
}

static struct bt_mesh_send_cb buf_sent_cb = {
	.start = buf_start,
	.end = buf_sent,
};

static uint8_t last_seg(uint8_t len)
{
	if (len <= START_PAYLOAD_MAX) {
		return 0;
	}

	len -= START_PAYLOAD_MAX;

	return 1 + (len / CONT_PAYLOAD_MAX);
}

static void free_segments(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pb_link.tx.buf); i++) {
		struct net_buf *buf = pb_link.tx.buf[i];

		if (!buf) {
			break;
		}

		pb_link.tx.buf[i] = NULL;
		/* Mark as canceled */
		BT_MESH_ADV(buf)->busy = 0U;
		net_buf_unref(buf);
	}
}

static uint8_t next_transaction_id(uint8_t id)
{
	return (((id + 1) & XACT_ID_MAX) | (id & (XACT_ID_MAX+1)));
}

static void prov_clear_tx(void)
{
	BT_DBG("");

	/* If this fails, the work handler will not find any buffers to send,
	 * and return without rescheduling. The work handler also checks the
	 * LINK_ACTIVE flag, so if this call is part of reset_adv_link, it'll
	 * exit early.
	 */
	(void)k_work_cancel_delayable(&pb_link.tx.retransmit);

	free_segments();
}

static void reset_adv_link(void)
{
	BT_DBG("");
	prov_clear_tx();

	/* If this fails, the work handler will exit early on the LINK_ACTIVE
	 * check.
	 */
	(void)k_work_cancel_delayable(&pb_link.prot_timer);

	if (atomic_test_bit(pb_link.flags, ADV_PROVISIONER)) {
		/* Clear everything except the retransmit and protocol timer
		 * delayed work objects.
		 */
		(void)memset(&pb_link, 0, offsetof(struct pb_adv, tx.retransmit));
		pb_link.rx.id = XACT_ID_NVAL;
	} else {
		/* Accept another provisioning attempt */
		pb_link.id = 0;
		atomic_clear(pb_link.flags);
		pb_link.rx.id = XACT_ID_MAX;
		pb_link.tx.id = XACT_ID_NVAL;
	}

	pb_link.tx.pending_ack = XACT_ID_NVAL;
	pb_link.rx.buf = &rx_buf;
	net_buf_simple_reset(pb_link.rx.buf);
}

static void close_link(enum prov_bearer_link_status reason)
{
	const struct prov_bearer_cb *cb = pb_link.cb;
	void *cb_data = pb_link.cb_data;

	reset_adv_link();
	cb->link_closed(&pb_adv, cb_data, reason);
}

static struct net_buf *adv_buf_create(uint8_t retransmits)
{
	struct net_buf *buf;

	buf = bt_mesh_adv_main_create(BT_MESH_ADV_PROV,
				      BT_MESH_TRANSMIT(retransmits, 20),
				      BUF_TIMEOUT);
	if (!buf) {
		BT_ERR("Out of provisioning buffers");
		return NULL;
	}

	return buf;
}

static void ack_complete(uint16_t duration, int err, void *user_data)
{
	BT_DBG("xact 0x%x complete", (uint8_t)pb_link.tx.pending_ack);
	atomic_clear_bit(pb_link.flags, ADV_ACK_PENDING);
}

static bool ack_pending(void)
{
	return atomic_test_bit(pb_link.flags, ADV_ACK_PENDING);
}

static void prov_failed(uint8_t err)
{
	BT_DBG("%u", err);
	pb_link.cb->error(&pb_adv, pb_link.cb_data, err);
	atomic_set_bit(pb_link.flags, ADV_LINK_INVALID);
}

static void prov_msg_recv(void)
{
	k_work_reschedule(&pb_link.prot_timer, PROTOCOL_TIMEOUT);

	if (!bt_mesh_fcs_check(pb_link.rx.buf, pb_link.rx.fcs)) {
		BT_ERR("Incorrect FCS");
		return;
	}

	gen_prov_ack_send(pb_link.rx.id);

	if (atomic_test_bit(pb_link.flags, ADV_LINK_INVALID)) {
		BT_WARN("Unexpected msg 0x%02x on invalidated pb_link",
			pb_link.rx.buf->data[0]);
		prov_failed(PROV_ERR_UNEXP_PDU);
		return;
	}

	pb_link.cb->recv(&pb_adv, pb_link.cb_data, pb_link.rx.buf);
}

static void protocol_timeout(struct k_work *work)
{
	if (!atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
		return;
	}

	BT_DBG("");

	pb_link.rx.seg = 0U;
	prov_link_close(PROV_BEARER_LINK_STATUS_TIMEOUT);
}
/*******************************************************************************
 * Generic provisioning
 ******************************************************************************/

static void gen_prov_ack_send(uint8_t xact_id)
{
	static const struct bt_mesh_send_cb cb = {
		.start = ack_complete,
	};
	const struct bt_mesh_send_cb *complete;
	struct net_buf *buf;
	bool pending = atomic_test_and_set_bit(pb_link.flags, ADV_ACK_PENDING);

	BT_DBG("xact_id 0x%x", xact_id);

	if (pending && pb_link.tx.pending_ack == xact_id) {
		BT_DBG("Not sending duplicate ack");
		return;
	}

	buf = adv_buf_create(RETRANSMITS_ACK);
	if (!buf) {
		atomic_clear_bit(pb_link.flags, ADV_ACK_PENDING);
		return;
	}

	if (pending) {
		complete = NULL;
	} else {
		pb_link.tx.pending_ack = xact_id;
		complete = &cb;
	}

	net_buf_add_be32(buf, pb_link.id);
	net_buf_add_u8(buf, xact_id);
	net_buf_add_u8(buf, GPC_ACK);

	bt_mesh_adv_send(buf, complete, NULL);
	net_buf_unref(buf);
}

static void gen_prov_cont(struct prov_rx *rx, struct net_buf_simple *buf)
{
	uint8_t seg = CONT_SEG_INDEX(rx->gpc);

	BT_DBG("len %u, seg_index %u", buf->len, seg);

	if (!pb_link.rx.seg && pb_link.rx.id == rx->xact_id) {
		if (!ack_pending()) {
			BT_DBG("Resending ack");
			gen_prov_ack_send(rx->xact_id);
		}

		return;
	}

	if (!pb_link.rx.seg &&
	    next_transaction_id(pb_link.rx.id) == rx->xact_id) {
		BT_DBG("Start segment lost");

		pb_link.rx.id = rx->xact_id;

		net_buf_simple_reset(pb_link.rx.buf);

		pb_link.rx.seg = SEG_NVAL;
		pb_link.rx.last_seg = SEG_NVAL;

		prov_clear_tx();
	} else if (rx->xact_id != pb_link.rx.id) {
		BT_WARN("Data for unknown transaction (0x%x != 0x%x)",
				rx->xact_id, pb_link.rx.id);
		return;
	}

	if (seg > pb_link.rx.last_seg) {
		BT_ERR("Invalid segment index %u", seg);
		prov_failed(PROV_ERR_NVAL_FMT);
		return;
	}

	if (!(pb_link.rx.seg & BIT(seg))) {
		BT_DBG("Ignoring already received segment");
		return;
	}

	memcpy(XACT_SEG_DATA(seg), buf->data, buf->len);
	XACT_SEG_RECV(seg);

	if (seg == pb_link.rx.last_seg && !(pb_link.rx.seg & BIT(0))) {
		uint8_t expect_len;

		expect_len = (pb_link.rx.buf->len - 20U -
				((pb_link.rx.last_seg - 1) * 23U));
		if (expect_len != buf->len) {
			BT_ERR("Incorrect last seg len: %u != %u", expect_len,
					buf->len);
			prov_failed(PROV_ERR_NVAL_FMT);
			return;
		}
	}

	if (!pb_link.rx.seg) {
		prov_msg_recv();
	}
}

static void gen_prov_ack(struct prov_rx *rx, struct net_buf_simple *buf)
{
	BT_DBG("len %u", buf->len);

	if (!pb_link.tx.buf[0]) {
		return;
	}

	if (rx->xact_id == pb_link.tx.id) {
		/* Don't clear resending of link_close messages */
		if (!atomic_test_bit(pb_link.flags, ADV_LINK_CLOSING)) {
			prov_clear_tx();
		}

		if (pb_link.tx.cb) {
			pb_link.tx.cb(0, pb_link.tx.cb_data);
		}
	}
}

static void gen_prov_start(struct prov_rx *rx, struct net_buf_simple *buf)
{
	uint8_t seg = SEG_NVAL;

	if (rx->xact_id == pb_link.rx.id) {
		if (!pb_link.rx.seg) {
			if (!ack_pending()) {
				BT_DBG("Resending ack");
				gen_prov_ack_send(rx->xact_id);
			}

			return;
		}

		if (!(pb_link.rx.seg & BIT(0))) {
			BT_DBG("Ignoring duplicate segment");
			return;
		}
	} else if (rx->xact_id != next_transaction_id(pb_link.rx.id)) {
		BT_WARN("Unexpected xact 0x%x, expected 0x%x", rx->xact_id,
			next_transaction_id(pb_link.rx.id));
		return;
	}

	net_buf_simple_reset(pb_link.rx.buf);
	pb_link.rx.buf->len = net_buf_simple_pull_be16(buf);
	pb_link.rx.id = rx->xact_id;
	pb_link.rx.fcs = net_buf_simple_pull_u8(buf);

	BT_DBG("len %u last_seg %u total_len %u fcs 0x%02x", buf->len,
	       START_LAST_SEG(rx->gpc), pb_link.rx.buf->len, pb_link.rx.fcs);

	if (pb_link.rx.buf->len < 1) {
		BT_ERR("Ignoring zero-length provisioning PDU");
		prov_failed(PROV_ERR_NVAL_FMT);
		return;
	}

	if (pb_link.rx.buf->len > pb_link.rx.buf->size) {
		BT_ERR("Too large provisioning PDU (%u bytes)",
		       pb_link.rx.buf->len);
		prov_failed(PROV_ERR_NVAL_FMT);
		return;
	}

	if (START_LAST_SEG(rx->gpc) > 0 && pb_link.rx.buf->len <= 20U) {
		BT_ERR("Too small total length for multi-segment PDU");
		prov_failed(PROV_ERR_NVAL_FMT);
		return;
	}

	prov_clear_tx();

	pb_link.rx.last_seg = START_LAST_SEG(rx->gpc);

	unsigned int msb_set = find_msb_set((~pb_link.rx.seg) & SEG_NVAL);

	if(msb_set != 0) {
		if ((pb_link.rx.seg & BIT(0)) && (msb_set - 1 > pb_link.rx.last_seg)) {
			BT_ERR("Invalid segment index %u", seg);
			prov_failed(PROV_ERR_NVAL_FMT);
			return;
		}
	}
	if (pb_link.rx.seg) {
		seg = pb_link.rx.seg;
	}

	pb_link.rx.seg = seg & ((1 << (START_LAST_SEG(rx->gpc) + 1)) - 1);
	memcpy(pb_link.rx.buf->data, buf->data, buf->len);
	XACT_SEG_RECV(0);

	if (!pb_link.rx.seg) {
		prov_msg_recv();
	}
}

static void gen_prov_ctl(struct prov_rx *rx, struct net_buf_simple *buf)
{
	BT_DBG("op 0x%02x len %u", BEARER_CTL(rx->gpc), buf->len);

	switch (BEARER_CTL(rx->gpc)) {
	case LINK_OPEN:
		link_open(rx, buf);
		break;
	case LINK_ACK:
		if (!atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
			return;
		}

		link_ack(rx, buf);
		break;
	case LINK_CLOSE:
		if (!atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
			return;
		}

		link_close(rx, buf);
		break;
	default:
		BT_ERR("Unknown bearer opcode: 0x%02x", BEARER_CTL(rx->gpc));

		if (IS_ENABLED(CONFIG_BT_TESTING)) {
			bt_test_mesh_prov_invalid_bearer(BEARER_CTL(rx->gpc));
		}

		return;
	}
}

static const struct {
	void (*func)(struct prov_rx *rx, struct net_buf_simple *buf);
	bool require_link;
	uint8_t min_len;
} gen_prov[] = {
	{ gen_prov_start, true, 3 },
	{ gen_prov_ack, true, 0 },
	{ gen_prov_cont, true, 0 },
	{ gen_prov_ctl, false, 0 },
};

static void gen_prov_recv(struct prov_rx *rx, struct net_buf_simple *buf)
{
	if (buf->len < gen_prov[GPCF(rx->gpc)].min_len) {
		BT_ERR("Too short GPC message type %u", GPCF(rx->gpc));
		return;
	}

	if (!atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE) &&
	    gen_prov[GPCF(rx->gpc)].require_link) {
		BT_DBG("Ignoring message that requires active link");
		return;
	}

	gen_prov[GPCF(rx->gpc)].func(rx, buf);
}

/*******************************************************************************
 * TX
 ******************************************************************************/

static void send_reliable(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pb_link.tx.buf); i++) {
		struct net_buf *buf = pb_link.tx.buf[i];

		if (!buf) {
			break;
		}

		if (BT_MESH_ADV(buf)->busy) {
			continue;
		}

		BT_DBG("%u bytes: %s", buf->len, bt_hex(buf->data, buf->len));

		bt_mesh_adv_send(buf, NULL, NULL);
	}

	k_work_reschedule(&pb_link.tx.retransmit, RETRANSMIT_TIMEOUT);
}

static void prov_retransmit(struct k_work *work)
{
	BT_DBG("");

	if (!atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
		BT_WARN("pb_link not active");
		return;
	}

	if (k_uptime_get() - pb_link.tx.start > TRANSACTION_TIMEOUT) {
		BT_WARN("Giving up transaction");
		prov_link_close(PROV_BEARER_LINK_STATUS_FAIL);
		return;
	}

	send_reliable();
}

static struct net_buf *ctl_buf_create(uint8_t op, const void *data, uint8_t data_len,
				      uint8_t retransmits)
{
	struct net_buf *buf;

	BT_DBG("op 0x%02x data_len %u", op, data_len);

	buf = adv_buf_create(retransmits);
	if (!buf) {
		return NULL;
	}

	net_buf_add_be32(buf, pb_link.id);
	/* Transaction ID, always 0 for Bearer messages */
	net_buf_add_u8(buf, 0x00);
	net_buf_add_u8(buf, GPC_CTL(op));
	net_buf_add_mem(buf, data, data_len);

	return buf;
}

static int bearer_ctl_send(struct net_buf *buf)
{
	if (!buf) {
		return -ENOMEM;
	}

	prov_clear_tx();
	k_work_reschedule(&pb_link.prot_timer, PROTOCOL_TIMEOUT);

	pb_link.tx.start = k_uptime_get();
	pb_link.tx.buf[0] = buf;
	send_reliable();

	return 0;
}

static int bearer_ctl_send_unacked(struct net_buf *buf)
{
	if (!buf) {
		return -ENOMEM;
	}

	prov_clear_tx();
	k_work_reschedule(&pb_link.prot_timer, PROTOCOL_TIMEOUT);

	bt_mesh_adv_send(buf, &buf_sent_cb, NULL);
	net_buf_unref(buf);

	return 0;
}

static int prov_send_adv(struct net_buf_simple *msg,
			 prov_bearer_send_complete_t cb, void *cb_data)
{
	struct net_buf *start, *buf;
	uint8_t seg_len, seg_id;

	prov_clear_tx();
	k_work_reschedule(&pb_link.prot_timer, PROTOCOL_TIMEOUT);

	start = adv_buf_create(RETRANSMITS_RELIABLE);
	if (!start) {
		return -ENOBUFS;
	}

	pb_link.tx.id = next_transaction_id(pb_link.tx.id);
	net_buf_add_be32(start, pb_link.id);
	net_buf_add_u8(start, pb_link.tx.id);

	net_buf_add_u8(start, GPC_START(last_seg(msg->len)));
	net_buf_add_be16(start, msg->len);
	net_buf_add_u8(start, bt_mesh_fcs_calc(msg->data, msg->len));

	pb_link.tx.buf[0] = start;
	pb_link.tx.cb = cb;
	pb_link.tx.cb_data = cb_data;
	pb_link.tx.start = k_uptime_get();

	BT_DBG("xact_id: 0x%x len: %u", pb_link.tx.id, msg->len);

	seg_len = MIN(msg->len, START_PAYLOAD_MAX);
	BT_DBG("seg 0 len %u: %s", seg_len, bt_hex(msg->data, seg_len));
	net_buf_add_mem(start, msg->data, seg_len);
	net_buf_simple_pull(msg, seg_len);

	buf = start;
	for (seg_id = 1U; msg->len > 0; seg_id++) {
		if (seg_id >= ARRAY_SIZE(pb_link.tx.buf)) {
			BT_ERR("Too big message");
			free_segments();
			return -E2BIG;
		}

		buf = adv_buf_create(RETRANSMITS_RELIABLE);
		if (!buf) {
			free_segments();
			return -ENOBUFS;
		}

		pb_link.tx.buf[seg_id] = buf;

		seg_len = MIN(msg->len, CONT_PAYLOAD_MAX);

		BT_DBG("seg %u len %u: %s", seg_id, seg_len,
		       bt_hex(msg->data, seg_len));

		net_buf_add_be32(buf, pb_link.id);
		net_buf_add_u8(buf, pb_link.tx.id);
		net_buf_add_u8(buf, GPC_CONT(seg_id));
		net_buf_add_mem(buf, msg->data, seg_len);
		net_buf_simple_pull(msg, seg_len);
	}

	send_reliable();

	return 0;
}

/*******************************************************************************
 * Link management rx
 ******************************************************************************/

static void link_open(struct prov_rx *rx, struct net_buf_simple *buf)
{
	int err;

	BT_DBG("len %u", buf->len);

	if (buf->len < 16) {
		BT_ERR("Too short bearer open message (len %u)", buf->len);
		return;
	}

	if (atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
		/* Send another link ack if the provisioner missed the last */
		if (pb_link.id != rx->link_id) {
			BT_DBG("Ignoring bearer open: link already active");
			return;
		}

		BT_DBG("Resending link ack");
		/* Ignore errors, message will be attempted again if we keep receiving link open: */
		(void)bearer_ctl_send_unacked(ctl_buf_create(LINK_ACK, NULL, 0, RETRANSMITS_ACK));
		return;
	}

	if (memcmp(buf->data, bt_mesh_prov_get()->uuid, 16)) {
		BT_DBG("Bearer open message not for us");
		return;
	}

	pb_link.id = rx->link_id;
	atomic_set_bit(pb_link.flags, ADV_LINK_ACTIVE);
	net_buf_simple_reset(pb_link.rx.buf);

	err = bearer_ctl_send_unacked(ctl_buf_create(LINK_ACK, NULL, 0, RETRANSMITS_ACK));
	if (err) {
		reset_adv_link();
		return;
	}

	pb_link.cb->link_opened(&pb_adv, pb_link.cb_data);
}

static void link_ack(struct prov_rx *rx, struct net_buf_simple *buf)
{
	BT_DBG("len %u", buf->len);

	if (atomic_test_bit(pb_link.flags, ADV_PROVISIONER)) {
		if (atomic_test_and_set_bit(pb_link.flags, ADV_LINK_ACK_RECVD)) {
			return;
		}

		prov_clear_tx();

		pb_link.cb->link_opened(&pb_adv, pb_link.cb_data);
	}
}

static void link_close(struct prov_rx *rx, struct net_buf_simple *buf)
{
	BT_DBG("len %u", buf->len);

	if (buf->len != 1) {
		return;
	}

	close_link(net_buf_simple_pull_u8(buf));
}

/*******************************************************************************
 * Higher level functionality
 ******************************************************************************/

void bt_mesh_pb_adv_recv(struct net_buf_simple *buf)
{
	struct prov_rx rx;

	if (!pb_link.cb) {
		return;
	}

	if (buf->len < 6) {
		BT_WARN("Too short provisioning packet (len %u)", buf->len);
		return;
	}

	rx.link_id = net_buf_simple_pull_be32(buf);
	rx.xact_id = net_buf_simple_pull_u8(buf);
	rx.gpc = net_buf_simple_pull_u8(buf);

	if (atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE) && pb_link.id != rx.link_id) {
		return;
	}

	BT_DBG("link_id 0x%08x xact_id 0x%x", rx.link_id, rx.xact_id);

	gen_prov_recv(&rx, buf);
}

static int prov_link_open(const uint8_t uuid[16], k_timeout_t timeout,
			  const struct prov_bearer_cb *cb, void *cb_data)
{
	int err;

	BT_DBG("uuid %s", bt_hex(uuid, 16));

	err = bt_mesh_adv_enable();
	if (err) {
		BT_ERR("Failed enabling advertiser");
		return err;
	}

	if (atomic_test_and_set_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
		return -EBUSY;
	}

	atomic_set_bit(pb_link.flags, ADV_PROVISIONER);

	bt_rand(&pb_link.id, sizeof(pb_link.id));
	pb_link.tx.id = XACT_ID_MAX;
	pb_link.rx.id = XACT_ID_NVAL;
	pb_link.cb = cb;
	pb_link.cb_data = cb_data;

	net_buf_simple_reset(pb_link.rx.buf);

	return bearer_ctl_send(ctl_buf_create(LINK_OPEN, uuid, 16, RETRANSMITS_RELIABLE));
}

static int prov_link_accept(const struct prov_bearer_cb *cb, void *cb_data)
{
	int err;

	err = bt_mesh_adv_enable();
	if (err) {
		BT_ERR("Failed enabling advertiser");
		return err;
	}

	if (atomic_test_bit(pb_link.flags, ADV_LINK_ACTIVE)) {
		return -EBUSY;
	}

	pb_link.rx.id = XACT_ID_MAX;
	pb_link.tx.id = XACT_ID_NVAL;
	pb_link.cb = cb;
	pb_link.cb_data = cb_data;

	/* Make sure we're scanning for provisioning invitations */
	bt_mesh_scan_enable();
	/* Enable unprovisioned beacon sending */
	bt_mesh_beacon_enable();

	return 0;
}

static void prov_link_close(enum prov_bearer_link_status status)
{
	if (atomic_test_and_set_bit(pb_link.flags, ADV_LINK_CLOSING)) {
		return;
	}

	/* Ignore errors, the link will time out eventually if this doesn't get sent */
	bearer_ctl_send_unacked(ctl_buf_create(LINK_CLOSE, &status, 1, RETRANSMITS_LINK_CLOSE));
}

void pb_adv_init(void)
{
	k_work_init_delayable(&pb_link.prot_timer, protocol_timeout);
	k_work_init_delayable(&pb_link.tx.retransmit, prov_retransmit);
}

void pb_adv_reset(void)
{
	reset_adv_link();
}

const struct prov_bearer pb_adv = {
	.type = BT_MESH_PROV_ADV,
	.link_open = prov_link_open,
	.link_accept = prov_link_accept,
	.link_close = prov_link_close,
	.send = prov_send_adv,
	.clear_tx = prov_clear_tx,
};
