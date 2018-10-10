#
# Makefile for misc devices that really don't fit anywhere else.
#
include $(srctree)/drivers/misc/mediatek/Makefile.custom

#obj-y += sensordriver/sensordrive_lens.o

ifeq ($(CONFIG_ARCH_MT6582),y)
	obj-$(CONFIG_MTK_LENS_DUMMYLENS_SUPPORT) +=  mt6582/dummy_lens/
        obj-$(CONFIG_MTK_LENS_LITEON3H7AF_SUPPORT) += mt6582/liteon3h7af/
	obj-$(CONFIG_MTK_LENS_FM50AF_SUPPORT)  +=  mt6582/fm50af/
endif
