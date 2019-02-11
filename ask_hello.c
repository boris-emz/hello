#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/genetlink.h>
#include "hello.h"

#define GENL_DATA(gh)	((void *)(((char *)(gh)) + GENL_HDRLEN))
#define NLA_DATA(na)	((void *)(((char *)(na)) + NLA_HDRLEN))

#define BUF_SZ 0x200
static char buf[BUF_SZ];

int socket_create(void)
{
	int fd;
	struct sockaddr_nl addr;

	fd = socket(AF_NETLINK,SOCK_RAW,NETLINK_GENERIC);
	if (fd < 0) {
		perror("socket");
		return -1;
	}
	memset(&addr,0,sizeof(addr));
	addr.nl_family = AF_NETLINK;
	if (bind(fd,(struct sockaddr *) &addr,
			sizeof(addr)) != 0) {
		perror("bind");
		goto no_bind;
	}
	return fd;
no_bind:
	close(fd);
	return -1;
}

void socket_close(int fd)
{
	close(fd);
}

int family_id(int fd)
{
	static char const name[] = HELLO_FAMILY_NAME;
	static int const name_sz = sizeof(name);

	struct sockaddr_nl addr;
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct nlattr *na;
	struct nlmsgerr *nle;
	int id;
	int len;
	int n;

	id = 0;
	len = 0;

	nlh = (struct nlmsghdr *) &buf[0];
	memset(nlh,0,NLMSG_HDRLEN);
	nlh->nlmsg_type = GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = getpid();
	len += NLMSG_HDRLEN;

	gh = (struct genlmsghdr *) NLMSG_DATA(nlh);
	memset(gh,0,GENL_HDRLEN);
	gh->cmd = CTRL_CMD_GETFAMILY;
	len += GENL_HDRLEN;

	na = (struct nlattr *) GENL_DATA(gh);
	memset(na,0,NLA_HDRLEN);
	na->nla_len = NLA_HDRLEN + name_sz;
	na->nla_type = CTRL_ATTR_FAMILY_NAME;
	memcpy(((char *) na) + NLA_HDRLEN,name,name_sz);
	len += na->nla_len;

	nlh->nlmsg_len = len;
	/*fprintf(stderr,"nlmsg_len %d\n",nlh->nlmsg_len);*/

	memset(&addr,0,sizeof(addr));
	addr.nl_family = AF_NETLINK;
	n = sendto(fd,buf,len,0,(struct sockaddr *) &addr,
			sizeof(addr));
	if (n != len) {
		perror("sendto");
		goto on_out;
	}
	/*fprintf(stderr,"sendto %d\n",n);*/

	n = recv(fd, buf, BUF_SZ,0);
	if (n < 0) {
		perror("recv");
		goto on_out;
	}

	len = 0;
	nlh = (struct nlmsghdr *) &buf[0];
	if (!NLMSG_OK(nlh,n)) {
		errno = EINVAL;
		perror("recv.nlmsg");
		goto on_out;
	}
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		errno = EINVAL;
		perror("recv.nlmsg_type");
		nle = (struct nlmsgerr *) NLMSG_DATA(nlh);
		fprintf(stderr,"\n error %d,msg ",nle->error);
		goto on_out;
	}
	len + NLMSG_HDRLEN;

	gh = (struct genlmsghdr *) NLMSG_DATA(nlh);
	if (gh->cmd != CTRL_CMD_NEWFAMILY) {
		errno = EINVAL;
		perror("recv.genl.cmd");
		goto on_out;
	}
	len += GENL_HDRLEN;

	na = (struct nlattr *) GENL_DATA(gh);
	if (!(len + NLA_HDRLEN <= n)) {
		errno = EINVAL;
		perror("recv.na.len");
		goto on_out;
	}
	len += NLA_ALIGN(na->nla_len);
	na = (struct nlattr *) (((char *) na) + NLA_ALIGN(na->nla_len));
	if (!(len + NLA_HDRLEN <= n)) {
		errno = EINVAL;
		perror("recv.na.len");
		goto on_out;
	}

	if (na->nla_type != CTRL_ATTR_FAMILY_ID) {
		errno = EINVAL;
		perror("recv.family");
		goto on_out;
	}
	if (!(len + NLA_HDRLEN + na->nla_len < n)) {
		errno = EINVAL;
		perror("recv.na.len");
		goto on_out;
	}

	id = *(__u16 *) NLA_DATA(na);
on_out:
	return id;
}

static char const *ask_hello(int fd,int id)
{
	struct sockaddr_nl addr;
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct nlattr *na;
	struct nlmsgerr *nle;
	char const *s;
	int len;
	int n;

	s = NULL;
	len = 0;

	nlh = (struct nlmsghdr *) &buf[0];
	memset(nlh,0,NLMSG_HDRLEN);
	nlh->nlmsg_type = (__u16) id;
	nlh->nlmsg_flags = NLM_F_REQUEST|NLM_F_DUMP;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = getpid();
	len += NLMSG_HDRLEN;

	gh = (struct genlmsghdr *) NLMSG_DATA(nlh);
	memset(gh,0,GENL_HDRLEN);
	gh->cmd = HELLO_CMD_ASK;
	len += GENL_HDRLEN;

	nlh->nlmsg_len = len;
	/* fprintf(stderr,"nlmsg_len %d\n",nlh->nlmsg_len);*/

	memset(&addr,0,sizeof(addr));
	addr.nl_family = AF_NETLINK;
	n = sendto(fd,buf,len,0,(struct sockaddr *) &addr,
			sizeof(addr));
	if (n != len) {
		perror("sendto");
		goto no_send;
	}

	n = recv(fd,buf,BUF_SZ,0);
	if (n < 0) {
		perror("recv");
		goto no_rcv;
	}
	if (!NLMSG_OK(nlh,n)) {
		errno = EINVAL;
		perror("recv.ok");
		goto no_rcv;
	}
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		errno = EINVAL;
		perror("recv.error");
		nle = (struct nlmsgerr *) NLMSG_DATA(nlh);
		fprintf(stderr,"\n error %d,msg ",nle->error);
		goto no_rcv;
	}

	n = NLMSG_PAYLOAD(nlh,0) - GENL_HDRLEN;
	gh = (struct genlmsghdr *) NLMSG_DATA(nlh);
	na = (struct nlattr *) GENL_DATA(gh);
	if (n < NLA_ALIGN(na->nla_len)) {
		errno = EINVAL;
		perror("recv.na.sz");
		goto no_rcv;
	}
	if (na->nla_type != HELLO_ATTR_MSG) {
		errno = EINVAL;
		perror("rcv.na.type");
		goto no_rcv;
	}

	s = NLA_DATA(na);

no_rcv:
no_send:
	return s;
}

int main(int argc,char **argv)
{
	char const *s;
	int fd;
	int id;

	fd = socket_create();
	if (fd < 0)
		exit(-1);

	id = family_id(fd);
	if (!id)
		exit(-1);

	s = ask_hello(fd,id);
	if (!s)
		exit(-1);

	printf("%s\n",s);

	socket_close(fd);

	return 0;
}

