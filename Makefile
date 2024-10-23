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

CSRCS += lib/os/crc8_sw.c
CSRCS += lib/os/crc16_sw.c
CSRCS += lib/os/crc32_sw.c

SUBDIR = subsys/bluetooth
CSRCS += subsys/net/buf.c
CSRCS += port/common/defines.c
CSRCS += port/kernel/mempool.c

ifeq ($(CONFIG_BT_HCI),y)
  ifeq ($(CONFIG_BT_HCI_RAW),y)
    CSRCS += $(SUBDIR)/host/hci_raw.c
    CSRCS += $(SUBDIR)/host/hci_common.c
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
    CSRCS += $(SUBDIR)/host/avdtp_ep.c
  endif
  ifeq ($(CONFIG_BT_AVRCP_CTTG),y)
    CSRCS += $(SUBDIR)/host/avrcp_cttg.c
  endif
  ifeq ($(CONFIG_BT_AVRCP),y)
    CSRCS += $(SUBDIR)/host/avrcp.c
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
    CSRCS += $(SUBDIR)/host/br.c
    CSRCS += $(SUBDIR)/host/keys_br.c
    CSRCS += $(SUBDIR)/host/l2cap_br.c
    CSRCS += $(SUBDIR)/host/sdp.c
    CSRCS += $(SUBDIR)/host/ssp.c
  endif

  ifeq ($(CONFIG_BT_HFP_HF),y)
    CSRCS += $(SUBDIR)/host/hfp_hf.c
    CSRCS += $(SUBDIR)/host/at.c
  endif

  ifeq ($(CONFIG_BT_HCI_HOST),y)
    CSRCS += $(SUBDIR)/host/uuid.c
    CSRCS += $(SUBDIR)/host/addr.c
    CSRCS += $(SUBDIR)/host/buf.c
    CSRCS += $(SUBDIR)/host/hci_core.c
    CSRCS += $(SUBDIR)/host/hci_common.c
    CSRCS += $(SUBDIR)/host/id.c

    ifeq ($(CONFIG_BT_BROADCASTER),y)
      CSRCS += $(SUBDIR)/host/adv.c
    endif

    ifeq ($(CONFIG_BT_OBSERVER),y)
      CSRCS += $(SUBDIR)/host/scan.c
    endif

    ifeq ($(CONFIG_BT_HOST_CRYPTO),y)
      CSRCS += $(SUBDIR)/host/crypto.c
    else
      CSRCS += port/subsys/bluetooth/crypto.c
    endif

    ifeq ($(CONFIG_BT_ECC),y)
      CSRCS += $(SUBDIR)/host/ecc.c
    endif

    ifeq ($(CONFIG_BT_CONN),y)
      CSRCS += $(SUBDIR)/host/conn.c
      CSRCS += $(SUBDIR)/host/l2cap.c
      CSRCS += $(SUBDIR)/host/att.c
      CSRCS += $(SUBDIR)/host/gatt.c

      ifeq ($(CONFIG_BT_SMP),y)
        CSRCS += $(SUBDIR)/host/smp.c
        CSRCS += $(SUBDIR)/host/keys.c
      else
        CSRCS += $(SUBDIR)/host/smp_null.c
      endif
    endif
  endif

  ifeq ($(CONFIG_BT_ISO),y)
    CSRCS += $(SUBDIR)/host/iso.c
    CSRCS += $(SUBDIR)/host/conn.c
  endif

  ifeq ($(CONFIG_BT_DF),y)
    CSRCS += $(SUBDIR)/host/direction.c
  endif
endif

ifeq ($(CONFIG_BT_AUDIO),y)
  ifeq (($(CONFIG_BT_VOCS), y), ($(CONFIG_BT_VOCS_CLIENT), y))
    CSRCS += $(SUBDIR)/audio/vocs.c
  endif

  ifeq ($(CONFIG_BT_VOCS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/vocs_client.c
  endif

  ifeq (($(CONFIG_BT_AICS), y), ($(CONFIG_BT_AICS_CLIENT), y))
    CSRCS += $(SUBDIR)/audio/aics.c
  endif

  ifeq ($(CONFIG_BT_AICS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/aics_client.c
  endif

  ifeq (($(CONFIG_BT_VCS), y), ($(CONFIG_BT_VCS_CLIENT), y))
    CSRCS += $(SUBDIR)/audio/vcs.c
  endif

  ifeq ($(CONFIG_BT_VCS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/vcs_client.c
  endif

  ifeq ($(CONFIG_BT_MICS),y)
    CSRCS += $(SUBDIR)audio/mics.c
  endif

  ifeq (($(CONFIG_BT_MICS), y), ($(CONFIG_BT_MICS_CLIENT), y))
    CSRCS += $(SUBDIR)/audio/mics_client.c
  endif

  ifeq ($(CONFIG_BT_CCID),y)
    CSRCS += $(SUBDIR)audio/ccid.c
  endif

  ifeq ($(CONFIG_BT_CSIS),y)
    CSRCS += $(SUBDIR)audio/csis.c
  endif

  ifeq ($(CONFIG_BT_CSIS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/csis_client.c
  endif

  ifeq (($(CONFIG_BT_CSIS), y), ($(CONFIG_BT_CSIS_CLIENT), y))
    CSRCS += $(SUBDIR)/audio/csis_crypto.c
  endif

  ifeq ($(CONFIG_BT_TBS),y)
    CSRCS += $(SUBDIR)audio/tbs.c
  endif

  ifeq ($(CONFIG_BT_TBS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/tbs_client.c
  endif
  ifeq ($(CONFIG_BT_MCC),y)
    CSRCS += $(SUBDIR)audio/mcc.c
  endif
  ifeq ($(CONFIG_BT_MCS),y)
    CSRCS += $(SUBDIR)audio/mcs.c
  endif
  ifeq ($(CONFIG_BT_MPL),y)
    CSRCS += $(SUBDIR)audio/mpl.c
  endif
  ifeq ($(CONFIG_MCTL),y)
    CSRCS += $(SUBDIR)audio/media_proxy.c
  endif
  ifeq ($(CONFIG_BT_ASCS),y)
    CSRCS += $(SUBDIR)audio/ascs.c
  endif
  ifeq ($(CONFIG_BT_PACS),y)
    CSRCS += $(SUBDIR)audio/pacs.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_STREAM),y)
    CSRCS += $(SUBDIR)audio/stream.c
    CSRCS += $(SUBDIR)audio/codec.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_UNICAST_SERVER),y)
    CSRCS += $(SUBDIR)audio/unicast_server.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_CAPABILITIES),y)
    CSRCS += $(SUBDIR)audio/capabilities.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_UNICAST_CLIENT),y)
    CSRCS += $(SUBDIR)audio/unicast_client.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_BROADCAST_SOURCE),y)
    CSRCS += $(SUBDIR)audio/broadcast_source.c
  endif
  ifeq ($(CONFIG_BT_AUDIO_BROADCAST_SINK),y)
    CSRCS += $(SUBDIR)audio/broadcast_sink.c
  endif
  ifeq ($(CONFIG_BT_BASS),y)
    CSRCS += $(SUBDIR)audio/bass.c
  endif
  ifeq ($(CONFIG_BT_BASS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/bass_client.c
  endif
  ifeq ($(CONFIG_BT_HAS),y)
    CSRCS += $(SUBDIR)audio/has.c
  endif
  ifeq ($(CONFIG_BT_HAS_CLIENT),y)
    CSRCS += $(SUBDIR)audio/has_client.c
  endif
endif

ifeq ($(CONFIG_BT_CONN),y)
  ifeq ($(CONFIG_BT_DIS),y)
    CSRCS += $(SUBDIR)/services/dis.c
  endif

  ifeq ($(CONFIG_BT_BAS),y)
    CSRCS += $(SUBDIR)/services/bas.c
  endif

  ifeq ($(CONFIG_BT_HRS),y)
    CSRCS += $(SUBDIR)/services/hrs.c
  endif

  ifeq ($(CONFIG_BT_TPS),y)
    CSRCS += $(SUBDIR)/services/tps.c
  endif

  ifeq ($(CONFIG_BT_IAS),y)
    CSRCS += $(SUBDIR)/services/ias.c
  endif

  ifeq ($(CONFIG_BT_OTS),y)
    CSRCS += $(SUBDIR)/services/ots/ots.c
    CSRCS += $(SUBDIR)/services/ots/ots_l2cap.c
    CSRCS += $(SUBDIR)/services/ots/ots_obj_manager.c
    CSRCS += $(SUBDIR)/services/ots/ots_oacp.c
    CSRCS += $(SUBDIR)/services/ots/ots_olcp.c

    ifeq ($(CONFIG_BT_OTS_DIR_LIST_OBJ),y)
      CSRCS += $(SUBDIR)/services/ots/ots_dir_list.c
    endif

    ifeq ($(CONFIG_BT_OTS_CLIENT),y)
      CSRCS += $(SUBDIR)/services/ots/ots_client.c
      CSRCS += $(SUBDIR)/services/ots/ots_l2cap.c
    endif
  endif
endif

ifeq ($(CONFIG_BT_MESH),y)
  CSRCS += $(SUBDIR)/mesh/main.c
  CSRCS += $(SUBDIR)/mesh/cfg.c
  CSRCS += $(SUBDIR)/mesh/adv.c
  CSRCS += $(SUBDIR)/mesh/beacon.c
  CSRCS += $(SUBDIR)/mesh/net.c
  CSRCS += $(SUBDIR)/mesh/subnet.c
  CSRCS += $(SUBDIR)/mesh/app_keys.c
  CSRCS += $(SUBDIR)/mesh/transport.c
  CSRCS += $(SUBDIR)/mesh/rpl.c
  CSRCS += $(SUBDIR)/mesh/heartbeat.c
  CSRCS += $(SUBDIR)/mesh/crypto.c
  CSRCS += $(SUBDIR)/mesh/access.c
  CSRCS += $(SUBDIR)/mesh/msg.c
  CSRCS += $(SUBDIR)/mesh/cfg_srv.c
  CSRCS += $(SUBDIR)/mesh/health_srv.c

  ifeq ($(CONFIG_BT_MESH_ADV_LEGACY),y)
    CSRCS += $(SUBDIR)/mesh/adv_legacy.c
  endif
  ifeq ($(CONFIG_BT_MESH_ADV_EXT),y)
    CSRCS += $(SUBDIR)/mesh/adv_ext.c
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
  ifeq ($(CONFIG_BT_MESH_PROV_DEVICE),y)
    CSRCS += $(SUBDIR)/mesh/prov_device.c
  endif
  ifeq ($(CONFIG_BT_MESH_PROVISIONER),y)
    CSRCS += $(SUBDIR)/mesh/provisioner.c
  endif
  ifeq ($(CONFIG_BT_MESH_PB_ADV),y)
    CSRCS += $(SUBDIR)/mesh/pb_adv.c
  endif
  ifeq ($(CONFIG_BT_MESH_PB_GATT_COMMON),y)
    CSRCS += $(SUBDIR)/mesh/pb_gatt.c
  endif
  ifeq ($(CONFIG_BT_MESH_PB_GATT),y)
    CSRCS += $(SUBDIR)/mesh/pb_gatt_srv.c
  endif
  ifeq ($(CONFIG_BT_MESH_PB_GATT_CLIENT),y)
    CSRCS += $(SUBDIR)/mesh/pb_gatt_cli.c
  endif
  ifeq ($(CONFIG_BT_MESH_GATT_CLIENT),y)
    CSRCS += $(SUBDIR)/mesh/gatt_cli.c
  endif
  ifeq ($(CONFIG_BT_MESH_PROXY_CLIENT),y)
    CSRCS += $(SUBDIR)/mesh/proxy_cli.c
  endif
  ifeq ($(CONFIG_BT_MESH_GATT_PROXY),y)
    CSRCS += $(SUBDIR)/mesh/proxy_srv.c
  endif
  ifeq ($(CONFIG_BT_MESH_GATT),y)
    CSRCS += $(SUBDIR)/mesh/proxy_msg.c
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
endif

CSRCS += $(SUBDIR)/common/log.c

ifeq ($(CONFIG_BT_RPA),y)
  CSRCS += $(SUBDIR)/common/rpa.c
endif

ifeq ($(CONFIG_SETTINGS),y)
  CSRCS += subsys/settings/src/settings.c
  CSRCS += subsys/settings/src/settings_store.c
  CSRCS += subsys/settings/src/settings_init.c
  CSRCS += subsys/settings/src/settings_line.c
  ifeq ($(CONFIG_SETTINGS_FS),y)
    CSRCS += subsys/settings/src/settings_file.c
  endif
  ifeq ($(CONFIG_SETTINGS_NVS),y)
    CSRCS += subsys/fs/nvs/nvs.c
    CSRCS += port/subsys/flash/flash.c
    CSRCS += subsys/settings/src/settings_nvs.c
  endif
  CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/settings/include}
endif

ifeq ($(CONFIG_FILE_SYSTEM),y)
  CSRCS += port/subsys/fs/fs.c
endif

ifeq ($(CONFIG_BT_SHELL),y)
  CSRCS += $(SUBDIR)/shell/bt.c
  CSRCS += $(SUBDIR)/shell/hci.c
  CSRCS += subsys/shell/shell_utils.c

  ifeq ($(CONFIG_BT_MIBLE_TEST),y)
    CSRCS += $(SUBDIR)/shell/mible_test.c
  endif

  ifeq ($(CONFIG_BT_CONN),y)
    CSRCS += $(SUBDIR)/shell/gatt.c
  endif

  ifeq ($(CONFIG_BT_BREDR),y)
    CSRCS += $(SUBDIR)/shell/bredr.c
  endif

  ifeq ($(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL),y)
    CSRCS += $(SUBDIR)/shell/l2cap.c
  endif

  ifeq ($(CONFIG_BT_RFCOMM),y)
    CSRCS += $(SUBDIR)/shell/rfcomm.c
  endif

  ifeq ($(CONFIG_BT_ISO),y)
    CSRCS += $(SUBDIR)/shell/iso.c
  endif

  ifeq ($(CONFIG_BT_VCS),y)
    CSRCS += $(SUBDIR)/shell/vcs.c
  endif

  ifeq ($(CONFIG_BT_VCS_CLIENT),y)
    CSRCS += $(SUBDIR)/shell/vcs_client.c
  endif

  ifeq ($(CONFIG_BT_MICS),y)
    CSRCS += $(SUBDIR)/shell/mics.c
  endif

  ifeq ($(CONFIG_BT_MICS_CLIENT),y)
    CSRCS += $(SUBDIR)/shell/mics_client.c
  endif

  ifeq ($(CONFIG_BT_CSIS),y)
    CSRCS += $(SUBDIR)/shell/csis.c
  endif

  ifeq ($(CONFIG_BT_CSIS_CLIENT),y)
    CSRCS += $(SUBDIR)/shell/csis_client.c
  endif

  ifeq ($(CONFIG_BT_MCS),y)
    CSRCS += $(SUBDIR)/shell/mpl.c
  endif

  ifeq ($(CONFIG_BT_MCC),y)
    CSRCS += $(SUBDIR)/shell/mcc.c
  endif

  ifeq ($(CONFIG_BT_MCS),y)
    CSRCS += $(SUBDIR)/shell/media_controller.c
  endif

  ifeq ($(CONFIG_BT_AUDIO_STREAM),y)
    CSRCS += $(SUBDIR)/shell/audio.c
  endif

  ifeq ($(CONFIG_BT_BASS),y)
    CSRCS += $(SUBDIR)/shell/bass.c
  endif

  ifeq ($(CONFIG_BT_BASS_CLIENT),y)
    CSRCS += $(SUBDIR)/shell/bass_client.c
  endif

  ifeq ($(CONFIG_BT_MESH_SHELL),y)
    CSRCS += $(SUBDIR)/mesh/shell.c
    MAINSRC += tests/bluetooth/mesh_shell/src/main.c
    PROGNAME += mesh
  endif

  ifeq ($(CONFIG_SETTINGS_FS),y)
    CSRCS += subsys/fs/shell.c
  endif

  CSRCS += port/subsys/shell/shell.c
endif

CSRCS += port/subsys/power/reboot.c

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

  MAINSRC  += tests/bluetooth/tester/src/main.c
  PROGNAME += bttester

  CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" tests/bluetooth/tester/src}
endif

CSRCS += port/kernel/sched.c

CSRCS += port/kernel/timeout.c
ifeq ($(CONFIG_ZTEST_TIMEOUT),y)
  MAINSRC  += port/tests/kernel/test_timeout.c
  PROGNAME += test_timeout
endif

ifeq ($(CONFIG_ZEPHYR_WORK_QUEUE),y)
CSRCS += kernel/work.c
CSRCS += kernel/system_work_q.c
else
CSRCS += port/kernel/work_q.c
endif
ifeq ($(CONFIG_ZTEST_WORK_Q),y)
  PROGNAME += test_workq
  MAINSRC  += port/tests/kernel/test_workq.c
endif

CSRCS += port/kernel/poll.c
ifeq ($(CONFIG_ZTEST_POLL),y)
  MAINSRC  += port/tests/kernel/test_poll.c
  PROGNAME += test_poll
endif

CSRCS += port/kernel/mem_slab.c
ifeq ($(CONFIG_ZTEST_MEMSLAB),y)
  MAINSRC  += port/tests/kernel/test_mem_slab.c
  PROGNAME += test_memslab
endif

CSRCS += port/kernel/atomic_c.c

CSRCS += port/kernel/sem.c
CSRCS += port/kernel/mutex.c

CSRCS += port/kernel/queue.c
ifeq ($(CONFIG_ZTEST_QUEUE),y)
  MAINSRC  += port/tests/kernel/test_queue.c
  PROGNAME += test_queue
endif

CSRCS += port/kernel/thread.c
ifeq ($(CONFIG_ZTEST_THREAD),y)
  MAINSRC  += port/tests/kernel/test_thread.c
  PROGNAME += test_thread
endif

ifeq ($(CONFIG_ZTEST_NVM),y)
  MAINSRC  += port/tests/fs/test_nvm.c
  PROGNAME += test_nvm
endif

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

ifeq ($(CONFIG_BT_USERCHAN),y)
  CSRCS += port/drivers/bluetooth/hci/userchan.c
endif

ifeq ($(CONFIG_BT_NATIVE),y)
  CSRCS += port/drivers/bluetooth/hci/native.c
endif

CFLAGS += -Wno-format-zero-length -Wno-implicit-function-declaration 
CFLAGS += -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-variable 
CFLAGS += -Wno-format -Wno-pointer-sign -Wno-strict-prototypes -Wno-implicit-int -Wno-shadow
CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-undef

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" port/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" port/include/kernel/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" include/zephyr}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/host}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/services}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/mesh}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" subsys/bluetooth/common}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/tinycrypt/lib/include}

ifneq ($(CONFIG_BT_SAMPLE),)
  ifneq ($(CONFIG_BT_SAMPLE_BROADCASTER),)
    PROGNAME += broadcaster
    MAINSRC += samples/bluetooth/broadcaster/src/main.c
    CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" samples/bluetooth/broadcaster/src}
  endif

  ifneq ($(CONFIG_BT_SAMPLE_OBSERVER),)
    PROGNAME += observer
    MAINSRC += samples/bluetooth/observer/src/main.c
    CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" samples/bluetooth/observer/src}
  endif


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
    ifeq ($(CONFIG_BT_MESH_PROV),)
      MAINSRC += samples/bluetooth/mesh_demo/src/main.c
    else
      CSRCS += samples/bluetooth/mesh_test/src/board.c
      MAINSRC += samples/bluetooth/mesh_test/src/main.c
    endif
    ifneq ($(CONFIG_BT_MESH_PROVISIONER),)
      PROGNAME += mesh_provisioner
      MAINSRC += samples/bluetooth/mesh_provisioner/src/main.c
    endif
  endif
endif

ifeq ($(CONFIG_BT_H4_ENABLE),y)
  PROGNAME += hci_uart
  MAINSRC += port/drivers/bluetooth/hci/h4_uart.c
endif

CSRCS  += port/drivers/init.c

PRIORITY  = SCHED_PRIORITY_DEFAULT
ifneq ($(CONFIG_BT_SAMPLE),)
  STACKSIZE = $(CONFIG_BT_SAMPLE_STACKSIZE)
  MODULE    = $(CONFIG_BT_SAMPLE)
else
  STACKSIZE = $(CONFIG_DEFAULT_TASK_STACKSIZE)
  MODULE    = n
endif

context::

NSH_FILE		= $(APPDIR)/nshlib/nsh_command.c
NSH_CMD_END_MARK	= '  { NULL,       NULL,         1, 1, NULL }'
NSH_ZBLUE_HEADER	= '\#ifdef CONFIG_BT_SHELL\n'
NSH_ZBLUE_CMD		= '  { \"zblue\",    cmd_zblue,    1, CONFIG_NSH_MAXARGUMENTS, \"zblue\" },\n'
NSH_ZBLUE_END		= '\#endif'
NSH_ZBLUE		= '\n'$(NSH_ZBLUE_HEADER)$(NSH_ZBLUE_CMD)$(NSH_ZBLUE_END)
NSH_CMD_END_MARK1	= '  { NULL,       NULL,         0, 0, NULL }'

NSH_EXTERN_ZBLUE	= 'extern int cmd_zblue(FAR struct nsh_vtbl_s *vtbl, int argc, char *argv[]);\n'
NSH_ZBLUE_EXTERN	= $(NSH_ZBLUE_HEADER)$(NSH_EXTERN_ZBLUE)$(NSH_ZBLUE_END)
NSH_POS1		= 'static int cmd_unrecognized'
NSH_POS			= 'static int  cmd_unrecognized'

depend::
ifeq ($(CONFIG_BT_SHELL),y)
	$(Q) sed 's/'$(NSH_POS)'/'$(NSH_ZBLUE_EXTERN)'\n\n'$(NSH_POS1)'/g' -i $(NSH_FILE)
	$(Q) sed 's/'$(NSH_CMD_END_MARK)'/'$(NSH_ZBLUE)'\n\n'$(NSH_CMD_END_MARK1)'/g' -i $(NSH_FILE)
endif

clean::
	$(call DELFILE, .built)

include $(APPDIR)/Application.mk
