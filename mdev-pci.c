/*
 * An mdev PCI stub driver
 * 
 * This driver can be bound to any PCI device for the purpose of exercising
 * mdev creation and removal.  No access is made to the PCI device, nor do
 * the mdev devices support any functionality, they cannot currently be
 * opened for any purpose, ie. assignment to QEMU or the like will fail.
 *
 * Copyright 2020 Red Hat, Inc
 *
 * Author: Alex Williamson <alex.williamson@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/mdev.h>
#include <linux/vfio.h>

const struct attribute_group *mdev_dev_groups[] = {
	NULL,
};

static ssize_t name_show(struct kobject *kobj, struct device *dev, char *buf)
{
	return sprintf(buf, "%s\n", kobj->name);
}
MDEV_TYPE_ATTR_RO(name);

static ssize_t description_show(struct kobject *kobj,
				struct device *dev, char *buf)
{
	return sprintf(buf, "mdev-pci mdev\n");
}
MDEV_TYPE_ATTR_RO(description);

static ssize_t available_instances_show(struct kobject *kobj,
					struct device *dev, char *buf)
{
	return sprintf(buf, "%d\n", 1); /* Can always make one more */
}
MDEV_TYPE_ATTR_RO(available_instances);

static ssize_t device_api_show(struct kobject *kobj,
			       struct device *dev, char *buf)
{
	return sprintf(buf, "%s\n", VFIO_DEVICE_API_PCI_STRING);
}
MDEV_TYPE_ATTR_RO(device_api);

static struct attribute *mdev_types_attrs[] = {
	&mdev_type_attr_name.attr,
	&mdev_type_attr_description.attr,
	&mdev_type_attr_device_api.attr,
	&mdev_type_attr_available_instances.attr,
	NULL,
};

static struct attribute_group mdev_type_group0 = {
	.name  = "0",
	.attrs = mdev_types_attrs,
};

static struct attribute_group mdev_type_group1 = {
	.name  = "1",
	.attrs = mdev_types_attrs,
};

static struct attribute_group *mdev_type_groups[] = {
	&mdev_type_group0,
	&mdev_type_group1,
	NULL,
};

static int mdev_create(struct kobject *kobj, struct mdev_device *mdev)
{
	return 0;
}

static int mdev_remove(struct mdev_device *mdev)
{
	return 0;
}

static const struct mdev_parent_ops mdev_pci_ops = {
	.owner			= THIS_MODULE,
	.mdev_attr_groups	= mdev_dev_groups,
	.supported_type_groups	= mdev_type_groups,
	.create			= mdev_create,
	.remove			= mdev_remove,
};

static int mdev_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	return mdev_register_device(&pdev->dev, &mdev_pci_ops);
}

static void mdev_pci_remove(struct pci_dev *pdev)
{
	mdev_unregister_device(&pdev->dev);
}

static struct pci_driver mdev_pci_driver = {
	.name		= "mdev_pci",
	.id_table	= NULL, /* only dynamic ids */
	.probe		= mdev_pci_probe,
	.remove		= mdev_pci_remove,
};

static char ids[1024] __initdata;
module_param_string(ids, ids, sizeof(ids), 0);
MODULE_PARM_DESC(ids, "Initial PCI IDs to add to the driver, format is \"vendor:device[:subvendor[:subdevice[:class[:class_mask]]]]\" and multiple comma separated entries can be specified");

static void __init mdev_pci_fill_ids(void)
{
	char *p, *id;
	int rc;

	/* no ids passed actually */
	if (ids[0] == '\0')
		return;

	/* add ids specified in the module parameter */
	p = ids;
	while ((id = strsep(&p, ","))) {
		unsigned int vendor, device, subvendor = PCI_ANY_ID,
		subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
		int fields;

		if (!strlen(id))
			continue;

		fields = sscanf(id, "%x:%x:%x:%x:%x:%x",
				&vendor, &device, &subvendor, &subdevice,
				&class, &class_mask);

		if (fields < 2) {
			pr_warn("invalid id string \"%s\"\n", id);
			continue;
		}

		rc = pci_add_dynid(&mdev_pci_driver, vendor, device,
				   subvendor, subdevice, class, class_mask, 0);
		if (rc)
			pr_warn("failed to add dynamic id [%04x:%04x[%04x:%04x]] class %#08x/%08x (%d)\n",
				vendor, device, subvendor, subdevice,
				class, class_mask, rc);
		else
			pr_info("add [%04x:%04x[%04x:%04x]] class %#08x/%08x\n",
				vendor, device, subvendor, subdevice,
				class, class_mask);
	}
}

static int __init mdev_pci_init(void)
{
	int ret = pci_register_driver(&mdev_pci_driver);

	if (ret)
		return ret;

	mdev_pci_fill_ids();
	return 0;
}

static void __exit mdev_pci_cleanup(void)
{
	pci_unregister_driver(&mdev_pci_driver);
}

module_init(mdev_pci_init);
module_exit(mdev_pci_cleanup);
MODULE_LICENSE("GPL v2");
