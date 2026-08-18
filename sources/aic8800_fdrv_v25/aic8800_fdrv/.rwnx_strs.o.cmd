cmd_/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o := arm-linux-gnueabi-gcc -Wp,-MD,/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/.rwnx_strs.o.d  -nostdinc -isystem /opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/../lib/gcc/arm-linux-gnueabi/7.2.1/include -I./arch/arm/include -I./arch/arm/include/generated  -I./include -I./arch/arm/include/uapi -I./arch/arm/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Wno-format-security -std=gnu89 -fno-dwarf2-cfi-asm -fno-ipa-sra -mabi=aapcs-linux -mfpu=vfp -funwind-tables -marm -Wa,-mno-warn-deprecated -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -Uarm -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wno-unused-but-set-variable -Wimplicit-fallthrough -Wno-unused-const-variable -fomit-frame-pointer -fno-var-tracking-assignments -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -DCONFIG_RWNX_DEBUGFS -DCONFIG_RWNX_UM_HELPER_DFLT=\""/dini/dini_bin/rwnx_umh.sh"\" -DCONFIG_RWNX_P2P_DEBUGFS -DNX_VIRT_DEV_MAX=4 -DNX_REMOTE_STA_MAX_FOR_OLD_IC=8 -DNX_REMOTE_STA_MAX=32 -DNX_MU_GROUP_MAX=62 -DNX_TXDESC_CNT=64 -DNX_TX_MAX_RATES=4 -DNX_CHAN_CTXT_CNT=3 -DCONFIG_START_FROM_BOOTROM -DCONFIG_VRF_DCDC_MODE -DCONFIG_ROM_PATCH_EN -DCONFIG_COEX -DCONFIG_RWNX_FULLMAC -I/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/. -DCONFIG_RWNX_RADAR -DCONFIG_RFTEST -DCONFIG_MCC -DAICWF_USB_SUPPORT -DCONFIG_USER_MAX=1 -DNX_TXQ_CNT=5 -DAICWF_RX_REORDER -DAICWF_ARP_OFFLOAD -DUSE_5G -DCONFIG_USB_BT -DCONFIG_ALIGN_8BYTES -DCONFIG_USB_ALIGN_DATA -DCONFIG_MAC_RANDOM_IF_NO_MAC_IN_EFUSE -DDEFAULT_COUNTRY_CODE=""\"00""\" -DCONFIG_RX_NETIF_RECV_SKB -DCONFIG_USB_MSG_OUT_EP -DCONFIG_USB_MSG_IN_EP -DCONFIG_USE_USB_ZERO_PACKET -DCONFIG_SUPPORT_REALTIME_CHANGE_MAC -DCONFIG_PREALLOC_TXQ -DCONFIG_USE_WIRELESS_EXT -DCONFIG_DPD -DCONFIG_FORCE_DPD_CALIB -DCONFIG_DPD -Wno-implicit-fallthrough  -DMODULE  -DKBUILD_BASENAME='"rwnx_strs"' -DKBUILD_MODNAME='"aic8800_fdrv"' -c -o /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.c

source_/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o := /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.c

deps_/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o := \
    $(wildcard include/config/rwnx/fullmac.h) \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  include/linux/compiler_attributes.h \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/lmac_msg.h \
    $(wildcard include/config/rwnx/fhost.h) \
    $(wildcard include/config/usb/bt.h) \
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/lmac_mac.h \
    $(wildcard include/config/he/for/old/kernel.h) \
    $(wildcard include/config/vht/for/old/kernel.h) \
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/lmac_types.h \
    $(wildcard include/config/rwnx/tl4.h) \
  include/generated/uapi/linux/version.h \
  include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  arch/arm/include/uapi/asm/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/arm/include/generated/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  include/linux/compiler_types.h \
  arch/arm/include/uapi/asm/posix_types.h \
  include/uapi/asm-generic/posix_types.h \
  include/linux/bits.h \
    $(wildcard include/config/cc/is/gcc.h) \
    $(wildcard include/config/gcc/version.h) \
  include/linux/const.h \
  include/vdso/const.h \
  include/uapi/linux/const.h \
  include/vdso/bits.h \
  include/linux/build_bug.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  arch/arm/include/asm/barrier.h \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/thumb2/kernel.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/fa526.h) \
    $(wildcard include/config/arm/heavy/mb.h) \
    $(wildcard include/config/arm/dma/mem/bufferable.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/spectre.h) \
  include/asm-generic/barrier.h \
  include/linux/kasan-checks.h \

/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o: $(deps_/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o)

$(deps_/home/wnk/LicheePi_Nano/third_party/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/rwnx_strs.o):
