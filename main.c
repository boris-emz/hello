#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <net/genetlink.h>
#include "hello.h"

#define HELLO_MSG "Hello, world"

static struct genl_family hello_genl_family = {
	.id = GENL_ID_GENERATE,
	.name = HELLO_FAMILY_NAME,
};

static int hello_genl_ask(struct sk_buff *skb,struct netlink_callback *cb)
{
	void *hdr;

	hdr = genlmsg_put(skb,NETLINK_CB(cb->skb).portid,cb->nlh->nlmsg_seq,
			&hello_genl_family,0,HELLO_CMD_RES);
	if (!hdr) {
		pr_err("%s: no hdr\n",__func__);
		goto no_hdr;
	}

	if (nla_put_string(skb, HELLO_ATTR_MSG, HELLO_MSG)) {
		pr_err("%s: no msg\n",__func__);
		goto no_msg;
	}

	genlmsg_end(skb,hdr);

on_out:
	pr_info("%s: skb.len %d\n",__func__,skb->len);
	return skb->len;

no_msg:
	genlmsg_cancel(skb,hdr);
no_hdr:
	goto on_out;
};

static struct genl_ops hello_genl_ops[] = {
	{
		.cmd = HELLO_CMD_ASK,
		.dumpit = hello_genl_ask,
	},
};

static int __init hello_init(void)
{
	int rc;

	rc = genl_register_family(&hello_genl_family);
	if (rc) {
		pr_err("%s: no family\n",__func__);
		goto no_family;
	}
	rc = genl_register_ops(&hello_genl_family,&hello_genl_ops[0]);
	if (rc) {
		pr_err("%s: no ops\n",__func__);
		goto no_ops;
	}
	pr_info("%s: %s/%d registered\n",__func__,
			hello_genl_family.name,hello_genl_family.id);
	return 0;

no_ops:
	genl_unregister_family(&hello_genl_family);
no_family:
	return rc;
}

static void __exit hello_exit(void)
{
	genl_unregister_family(&hello_genl_family);
	pr_info("%s: %s/%d unregistered\n",__func__,
			hello_genl_family.name,hello_genl_family.id);
}

module_init(hello_init)
module_exit(hello_exit)

MODULE_LICENSE("GPL");
MODULE_VERSION(HELLO_VERSION);
