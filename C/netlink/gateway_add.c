#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <net/if.h>

#define BUFLEN		4096

#define for_each_nlmsg(n, buf, len)					\
	for (n = (struct nlmsghdr*)buf;					\
	     NLMSG_OK(n, (uint32_t)len) && n->nlmsg_type != NLMSG_DONE;	\
	     n = NLMSG_NEXT(n, len))

#define for_each_rattr(n, buf, len)					\
	for (n = (struct rtattr*)buf; RTA_OK(n, len); n = RTA_NEXT(n, len))

static inline
void check(int val)
{
	/*
	 * NOTICE: we have not reclaim the resources allocated before, this is
	 * due to the fact that this program aims to illustrate the facts and
	 * principles, but not for production environment
	 */
	if (val < 0) {
		printf("check error: %s\n", strerror(errno));
		exit(1);
	}
}

static inline
int pton(int domain, void *buf, const char *ip)
{
	if (domain == AF_INET) {
		inet_pton(AF_INET, ip, buf);
		return sizeof(struct in_addr);
	}

	if (domain == AF_INET6) {
		inet_pton(AF_INET6, ip, buf);
		return sizeof(struct in6_addr);
	}

	return -1;
}


static
int add_gateway(int fd, struct sockaddr_nl *sa, int domain, const char *ip)
{
	char buf[BUFLEN];
	int ip_len = 0;

	memset(buf, 0, BUFLEN);

	// assemble the message according to the netlink protocol
	struct nlmsghdr *nl;
	nl = (struct nlmsghdr*)buf;
	nl->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nl->nlmsg_type = RTM_NEWROUTE;
	// we request kernel to send back ack for result checking
	nl->nlmsg_flags = NLM_F_REQUEST | NLM_F_REPLACE | NLM_F_ACK | NLM_F_CREATE;

	struct rtmsg *rt;
	rt = (struct rtmsg*)NLMSG_DATA(nl);
	rt->rtm_family = domain;
	rt->rtm_table = RT_TABLE_MAIN;
	rt->rtm_protocol = RTPROT_STATIC;
	rt->rtm_scope = RT_SCOPE_UNIVERSE;
	rt->rtm_type = RTN_UNICAST;

	struct rtattr *rta = (struct rtattr*)RTM_RTA(rt);
	rta->rta_type = RTA_GATEWAY;
	ip_len = pton(domain, RTA_DATA(rta), ip);
	rta->rta_len = RTA_LENGTH(ip_len);
	nl->nlmsg_len = NLMSG_ALIGN(nl->nlmsg_len) + rta->rta_len;

	int l = BUFLEN - nl->nlmsg_len;
	rta = (struct rtattr*)RTA_NEXT(rta, l);
	rta->rta_type = RTA_OIF;
	rta->rta_len = RTA_LENGTH(sizeof(int));
	*((int*)RTA_DATA(rta)) = 2;
	nl->nlmsg_len += rta->rta_len;

	// prepare struct msghdr for sending.
	struct iovec iov = { nl, nl->nlmsg_len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	// send netlink message to kernel.
	int r = sendmsg(fd, &msg, 0);
	return (r < 0) ? -1 : 0;
}

static
int get_msg(int fd, struct sockaddr_nl *sa, void *buf, size_t len)
{
	struct iovec iov;
	struct msghdr msg;
	iov.iov_base = buf;
	iov.iov_len = len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = sa;
	msg.msg_namelen = sizeof(*sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	return recvmsg(fd, &msg, 0);
}

static
uint32_t parse_nl_msg(void *buf, size_t len)
{
	struct nlmsghdr *nl = NULL;
	nl = (struct nlmsghdr*)buf;
	if (!NLMSG_OK(nl, len)) return 0;
	return nl->nlmsg_type;
}

int main(void)
{
	int fd = 0, len = 0;

	// First of all, we need to create a socket with the AF_NETLINK domain
	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	check(fd);

	struct sockaddr_nl sa;
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;

	add_gateway(fd, &sa, AF_INET, "172.16.0.1");

	// after sending, we need to check the result
	char buf[BUFLEN];
	uint32_t nl_msg_type;
	len = get_msg(fd, &sa, buf, BUFLEN);
	check(len);

	nl_msg_type = parse_nl_msg(buf, len);
	if (nl_msg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(buf);
		switch (err->error) {
		case 0:
			printf("Success\n");
			break;
		case -EADDRNOTAVAIL:
			printf("Failed\n");
			break;
		case -EPERM:
			printf("Permission denied\n");
			break;
		default:
			printf("%s\n", strerror(err->error));
		}
	}

	return 0;
}

