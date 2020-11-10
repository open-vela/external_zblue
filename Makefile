#
# Copyright (C) 2020 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include $(APPDIR)/Make.defs

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
  CSRCS += $(SUBDIR)/host/ssp.c
endif

ifeq ($(CONFIG_BT_HFP_HF),y)
  CSRCS += $(SUBDIR)/host/hfp_hf.c
  CSRCS += $(SUBDIR)/host/at.c
endif

ifeq ($(CONFIG_BT_AUDIO),y)
  ifeq ($(CONFIG_BT_ISO),y)
    CSRCS += $(SUBDIR)/host/audio/iso.c
  endif
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

ifeq ($(CONFIG_BT_DIS),y)
  CSRCS += $(SUBDIR)/services/dis.c
endif

ifeq ($(CONFIG_BT_BAS),y)
  CSRCS += $(SUBDIR)/services/bas.c
endif

ifeq ($(CONFIG_BT_HRS),y)
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
ifeq ($(CONFIG_BT_MESH_CDB),y)
  CSRCS += $(SUBDIR)/mesh/cdb.c
endif

CSRCS += $(wildcard $(SUBDIR)/common/*.c)

ifeq ($(CONFIG_SETTINGS),y)
  CSRCS += subsys/settings/src/settings.c
  CSRCS += subsys/settings/src/settings_store.c
  CSRCS += subsys/settings/src/settings_init.c
  CSRCS += subsys/settings/src/settings_line.c
  ifeq ($(CONFIG_SETTINGS_FS),y)
    CSRCS += subsys/settings/src/settings_file.c
  endif
  CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/settings/include}
endif

ifeq ($(CONFIG_FILE_SYSTEM),y)
  CSRCS += port/subsys/fs/fs.c
endif

ifeq ($(CONFIG_BT_SHELL),y)
  CSRCS += port/subsys/shell/shell.c

  CSRCS    += $(SUBDIR)/shell/bt.c
  CSRCS    += $(SUBDIR)/shell/hci.c
  MAINSRC  += port/$(SUBDIR)/shell/bt.c
  PROGNAME += bt
  port/$(SUBDIR)/shell/bt.c_CFLAGS    += -DCONFIG_BT_BT_SHELL
  port/$(SUBDIR)/shell/bt.c_CELFFLAGS += -DCONFIG_BT_BT_SHELL

  ifeq ($(CONFIG_BT_CONN),y)
    CSRCS    += $(SUBDIR)/shell/gatt.c
    MAINSRC  += port/$(SUBDIR)/shell/gatt.c
    PROGNAME += gatt
    port/$(SUBDIR)/shell/gatt.c_CFLAGS    += -DCONFIG_BT_GATT_SHELL
    port/$(SUBDIR)/shell/gatt.c_CELFFLAGS += -DCONFIG_BT_GATT_SHELL
  endif
  ifeq ($(CONFIG_BT_BREDR),y)
    CSRCS    += $(SUBDIR)/shell/bredr.c
    MAINSRC  += port/$(SUBDIR)/shell/bredr.c
    PROGNAME += bredr
    port/$(SUBDIR)/shell/bredr.c_CFLAGS    += -DCONFIG_BT_BREDR_SHELL
    port/$(SUBDIR)/shell/bredr.c_CELFFLAGS += -DCONFIG_BT_BREDR_SHELL
  endif
  ifeq ($(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL),y)
    CSRCS    += $(SUBDIR)/shell/l2cap.c
    MAINSRC  += port/$(SUBDIR)/shell/l2cap.c
    PROGNAME += l2cap
    port/$(SUBDIR)/shell/l2cap.c_CFLAGS    += -DCONFIG_BT_L2CAP_SHELL
    port/$(SUBDIR)/shell/l2cap.c_CELFFLAGS += -DCONFIG_BT_L2CAP_SHELL
  endif
  ifeq ($(CONFIG_BT_RFCOMM),y)
    CSRCS    += $(SUBDIR)/shell/rfcomm.c
    MAINSRC  += port/$(SUBDIR)/shell/rfcomm.c
    PROGNAME += rfcomm
    port/$(SUBDIR)/shell/rfcomm.c_CFLAGS    += -DCONFIG_BT_RFCOMM_SHELL
    port/$(SUBDIR)/shell/rfcomm.c_CELFFLAGS += -DCONFIG_BT_RFCOMM_SHELL
  endif

  ifeq ($(CONFIG_BT_AUDIO),y)
    ifeq ($(CONFIG_BT_ISO),y)
      CSRCS    += $(SUBDIR)/shell/iso.c
      MAINSRC  += port/$(SUBDIR)/shell/iso.c
      PROGNAME += iso
      port/$(SUBDIR)/shell/iso.c_CFLAGS    += -DCONFIG_BT_ISO_SHELL
      port/$(SUBDIR)/shell/iso.c_CELFFLAGS += -DCONFIG_BT_ISO_SHELL
    endif
  endif

  ifeq ($(CONFIG_BT_MESH),y)
    ifeq ($(CONFIG_BT_MESH_SHELL),y)
      CSRCS    += $(SUBDIR)/mesh/shell.c
      MAINSRC  += port/$(SUBDIR)/shell/mesh.c
      PROGNAME += mesh
    endif
  endif

endif

ifeq ($(CONFIG_BT_TESTER),y)
  CSRCS += tests/bluetooth/tester/src/bttester.c
  CSRCS += tests/bluetooth/tester/src/gap.c
  CSRCS += tests/bluetooth/tester/src/gatt.c
  ifeq ($(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL),y)
    CSRCS += tests/bluetooth/tester/src/l2cap.c
  endif
  ifeq ($(CONFIG_BT_MESH),y)
    CSRCS += tests/bluetooth/tester/src/mesh.c
  endif
  CSRCS += port/drivers/console/uart_pipe.c
  CSRCS += port/tests/bluetooth/tester/src/system.c

  CSRCS += port/subsys/power/reboot.c

  MAINSRC  += tests/bluetooth/tester/src/main.c
  PROGNAME += bttester

  CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" tests/bluetooth/tester/src}
endif

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

ifeq ($(CONFIG_ARCH_CHIP_R328),y)
  CSRCS += port/chip/xr829/xr829.c
  CFLAGS += ${shell $(INCDIR) "$(CC)" $(TOPDIR)/arch/arm/src/r328/include/drivers}
  CFLAGS += ${shell $(INCDIR) "$(CC)" $(TOPDIR)/arch/arm/src/r328/include}
endif

ifeq ($(CONFIG_BT_LIBUSB),y)
  CSRCS += port/drivers/bluetooth/hci/libusb.c
endif

ifeq ($(CONFIG_BT_H4),y)
  CSRCS += port/drivers/bluetooth/hci/h4.c
endif

CFLAGS += -Wno-implicit-function-declaration -Wno-unused-but-set-variable -Wno-unused-function

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" port/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/host}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/services}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/mesh}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/common}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/tinycrypt/lib/include}

ifeq ($(CONFIG_ARCH_SIM),y)
  CFLAGS += -O2 -fno-strict-aliasing
  CFLAGS += -ffunction-sections -fdata-sections
endif

ifneq ($(CONFIG_BT_SAMPLE),)

  ifneq ($(CONFIG_BT_SAMPLE_PERIPHERAL),)
    PROGNAME += peripheral
    CSRCS += samples/bluetooth/peripheral/src/cts.c
    MAINSRC += samples/bluetooth/peripheral/src/main.c
    CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" samples/bluetooth/peripheral/src}
  endif

  ifneq ($(CONFIG_BT_SAMPLE_CENTRAL),)
    PROGNAME += central
    MAINSRC += samples/bluetooth/central/src/main.c
  endif

  ifneq ($(CONFIG_BT_SAMPLE_MESH),)
    PROGNAME += btmesh
    MAINSRC += samples/bluetooth/mesh/src/main.c
    PROGNAME += mesh_demo
    MAINSRC += samples/bluetooth/mesh_demo/src/main.c
    ifneq ($(CONFIG_BT_MESH_PROVISIONER),)
      PROGNAME += mesh_provisioner
      MAINSRC += samples/bluetooth/mesh_provisioner/src/main.c
    endif
  endif

endif

PROGNAME += zblue
MAINSRC  += port/drivers/bluetooth/hci/hci.c

PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_BT_SAMPLE_STACKSIZE)
MODULE    = $(CONFIG_BT_SAMPLE)

depend::
	$(Q) ln -sf ../../include/bluetooth port/include/bluetooth
	$(Q) ln -sf ../../include/drivers port/include/drivers
	$(Q) ln -sf ../../include/fs port/include/fs
	$(Q) ln -sf ../../include/settings port/include/settings
	$(Q) ln -sf ../../include/power port/include/power
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/bredr.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/bt.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/gatt.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/l2cap.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/mesh.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/rfcomm.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/ticker.c
	$(Q) ln -sf shell.c port/subsys/bluetooth/shell/iso.c

clean::
	$(call DELFILE, .built)
	$(call DELFILE, port/include/bluetooth)
	$(call DELFILE, port/include/drivers)
	$(call DELFILE, port/include/fs)
	$(call DELFILE, port/include/settings)
	$(call DELFILE, port/include/power)
	$(call DELFILE, port/subsys/bluetooth/shell/bredr.c)
	$(call DELFILE, port/subsys/bluetooth/shell/bt.c)
	$(call DELFILE, port/subsys/bluetooth/shell/gatt.c)
	$(call DELFILE, port/subsys/bluetooth/shell/l2cap.c)
	$(call DELFILE, port/subsys/bluetooth/shell/mesh.c)
	$(call DELFILE, port/subsys/bluetooth/shell/rfcomm.c)
	$(call DELFILE, port/subsys/bluetooth/shell/ticker.c)
	$(call DELFILE, port/subsys/bluetooth/shell/iso.c)

include $(APPDIR)/Application.mk
