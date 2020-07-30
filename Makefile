############################################################################
# external/zblue/Makefile
#
#   Copyright (C) 2020 Xiaomi Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

-include $(APPDIR)/Make.defs

SUBDIR = subsys/bluetooth

ifeq ($(CONFIG_BT_HCI_RAW),y)
  CSRCS += $(SUBDIR)/host/hci_raw.c
endif
ifeq ($(CONFIG_BT_DEBUG_MONITOR),y)
  CSRCS += $(SUBDIR)/host/monitor.c
endif
ifeq ($(CONFIG_BT_TINYCRYPT_ECC),y)
  CSRCS += $(SUBDIR)/host/hci_ecc.c
endif
ifeq ($(CONFIG_BT_A2DP),y)
  CSRCS += $(SUBDIR)/host/a2dp.c
endif
ifeq ($(CONFIG_BT_AVDTP),y)
  CSRCS += $(SUBDIR)/host/avdtp.c
endif
ifeq ($(CONFIG_BT_RFCOMM),y)
  CSRCS += $(SUBDIR)/host/rfcomm.c
endif
ifeq ($(CONFIG_BT_TESTING),y)
  CSRCS += $(SUBDIR)/host/testing.c
endif
ifeq ($(CONFIG_BT_SETTINGS),y)
  CSRCS += $(SUBDIR)/host/settings.c
endif
ifeq ($(CONFIG_BT_HOST_CCM),y)
  CSRCS += $(SUBDIR)/host/aes_ccm.c
endif

ifeq ($(CONFIG_BT_BREDR),y)
  CSRCS += $(SUBDIR)/host/keys_br.c
  CSRCS += $(SUBDIR)/host/l2cap_br.c
  CSRCS += $(SUBDIR)/host/sdp.c
endif

ifeq ($(CONFIG_BT_HFP_HF),y)
  CSRCS += $(SUBDIR)/host/hfp_hf.c
  CSRCS += $(SUBDIR)/host/at.c
endif


ifeq ($(CONFIG_BT_HCI_HOST),y)
  CSRCS += $(SUBDIR)/host/uuid.c
  CSRCS += $(SUBDIR)/host/hci_core.c

  ifeq ($(CONFIG_BT_HOST_CRYPTO),y)
    CSRCS += $(SUBDIR)/host/crypto.c
  endif

  ifeq ($(CONFIG_BT_CONN),y)
    CSRCS += $(SUBDIR)/host/conn.c
    CSRCS += $(SUBDIR)/host/l2cap.c
    CSRCS += $(SUBDIR)/host/att.c
    CSRCS += $(SUBDIR)/host/gatt.c
  endif

  ifeq ($(CONFIG_BT_SMP),y)
    CSRCS += $(SUBDIR)/host/smp.c
    CSRCS += $(SUBDIR)/host/keys.c
  else
    CSRCS += $(SUBDIR)/host/smp_null.c
  endif

endif

ifeq ($(CONFIG_BT_GATT_DIS),y)
  CSRCS += $(SUBDIR)/services/dis.c
endif

ifeq ($(CONFIG_BT_GATT_BAS),y)
  CSRCS += $(SUBDIR)/services/bas.c
endif

ifeq ($(CONFIG_BT_GATT_HRS),y)
  CSRCS += $(SUBDIR)/services/hrs.c
endif

ifeq ($(CONFIG_BT_MESH),y)
  CSRCS += $(SUBDIR)/mesh/main.c
  CSRCS += $(SUBDIR)/mesh/adv.c
  CSRCS += $(SUBDIR)/mesh/beacon.c
  CSRCS += $(SUBDIR)/mesh/net.c
  CSRCS += $(SUBDIR)/mesh/transport.c
  CSRCS += $(SUBDIR)/mesh/crypto.c
  CSRCS += $(SUBDIR)/mesh/access.c
  CSRCS += $(SUBDIR)/mesh/cfg_srv.c
  CSRCS += $(SUBDIR)/mesh/health_srv.c
endif

ifeq ($(CONFIG_BT_SETTINGS),y)
  CSRCS += $(SUBDIR)/mesh/settings.c
endif

ifeq ($(CONFIG_BT_MESH_LOW_POWER),y)
  CSRCS += $(SUBDIR)/mesh/lpn.c
endif
ifeq ($(CONFIG_BT_MESH_FRIEND),y)
  CSRCS += $(SUBDIR)/mesh/friend.c
endif
ifeq ($(CONFIG_BT_MESH_PROV),y)
  CSRCS += $(SUBDIR)/mesh/prov.c
endif
ifeq ($(CONFIG_BT_MESH_PB_ADV),y)
  CSRCS += $(SUBDIR)/mesh/pb_adv.c
endif
ifeq ($(CONFIG_BT_MESH_PB_GATT),y)
  CSRCS += $(SUBDIR)/mesh/pb_gatt.c
endif
ifeq ($(CONFIG_BT_MESH_PROXY),y)
  CSRCS += $(SUBDIR)/mesh/proxy.c
endif
ifeq ($(CONFIG_BT_MESH_CFG_CLI),y)
  CSRCS += $(SUBDIR)/mesh/cfg_cli.c
endif
ifeq ($(CONFIG_BT_MESH_HEALTH_CLI),y)
  CSRCS += $(SUBDIR)/mesh/health_cli.c
endif
ifeq ($(CONFIG_BT_MESH_SELF_TEST),y)
  CSRCS += $(SUBDIR)/mesh/test.c
endif
ifeq ($(CONFIG_BT_MESH_SHELL),y)
  CSRCS += $(SUBDIR)/mesh/shell.c
endif
ifeq ($(CONFIG_BT_MESH_CDB),y)
  CSRCS += $(SUBDIR)/mesh/cdb.c
endif

CSRCS += $(wildcard $(SUBDIR)/common/*.c)

CSRCS += port/kernel/sched.c
CSRCS += port/kernel/timeout.c
CSRCS += port/kernel/work_q.c
CSRCS += port/kernel/poll.c
CSRCS += port/kernel/mem_slab.c
CSRCS += port/kernel/atomic_c.c
CSRCS += port/kernel/sem.c
CSRCS += port/kernel/mutex.c
CSRCS += port/kernel/queue.c
CSRCS += port/kernel/thread.c
CSRCS += port/subsys/net/buf.c
CSRCS += port/common/defines.c

CSRCS += lib/os/dec.c
CSRCS += lib/os/hex.c

ifeq ($(CONFIG_ARCH_CHIP_AMEBAZ),y)
  CSRCS += port/chip/amebaz/amebaz.c
  CSRCS += port/chip/amebaz/amebaz_firmware.c
endif

ifeq ($(CONFIG_ARCH_CHIP_R328),y)
  CSRCS += port/chip/xr829/xr829.c
  CFLAGS += ${shell $(INCDIR) "$(CC)" $(TOPDIR)/arch/arm/src/r328/include/drivers}
  CFLAGS += ${shell $(INCDIR) "$(CC)" $(TOPDIR)/arch/arm/src/r328/include}
endif

ifeq ($(CONFIG_BT_LIBUSB),y)
  CSRCS += port/drivers/bluetooth/hci/libusb.c
endif

CFLAGS += -Wno-implicit-function-declaration -Wno-unused-but-set-variable -Wno-unused-function

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/subsys/bluetooth}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/subsys/bluetooth/host}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/subsys/bluetooth/services}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/subsys/bluetooth/mesh}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/subsys/bluetooth/common}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/tinycrypt/lib/include}

ifeq ($(CONFIG_ARCH_SIM),y)
  CFLAGS += -O2 -fno-strict-aliasing
  CFLAGS += -ffunction-sections -fdata-sections
endif

ifneq ($(CONFIG_BT_SAMPLE),)

  PRIORITY = SCHED_PRIORITY_DEFAULT
  STACKSIZE = $(CONFIG_BT_SAMPLE_STACKSIZE)
  MODULE = $(CONFIG_BT_SAMPLE)

  ifneq ($(CONFIG_BT_SAMPLE_PERIPHERAL),)
    PROGNAME += peripheral
    CSRCS += samples/bluetooth/peripheral/src/cts.c
    MAINSRC += samples/bluetooth/peripheral/src/main.c
    CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/samples/bluetooth/peripheral/src}
  endif

  ifneq ($(CONFIG_BT_SAMPLE_CENTRAL),)
    PROGNAME += central
    MAINSRC += samples/bluetooth/central/src/main.c
  endif

  ifneq ($(CONFIG_BT_SAMPLE_MESH),)
    PROGNAME += mesh
    MAINSRC += samples/bluetooth/mesh/src/main.c
    PROGNAME += mesh_demo
    MAINSRC += samples/bluetooth/mesh_demo/src/main.c
    ifneq ($(CONFIG_BT_MESH_PROVISIONER),)
      PROGNAME += mesh_provisioner
      MAINSRC += samples/bluetooth/mesh_provisioner/src/main.c
    endif
  endif

endif

include $(APPDIR)/Application.mk
