obj-m += sunxi_ion_core.o

sunxi_ion_core-y +=	ion.o ion-ioctl.o ion_heap.o \
			ion_page_pool.o ion_system_heap.o \
			ion_carveout_heap.o ion_chunk_heap.o ion_cma_heap.o \
			ion_of.o sunxi/sunxi_ion.o
ifdef CONFIG_COMPAT
sunxi_ion_core-y += compat_ion.o
endif
