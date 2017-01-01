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
char *ntop(int domain, void *buf)
{
	/*
	 * this function is not thread safe
	 */
	static char ip[INET6_ADDRSTRLEN];
	inet_ntop(domain, buf, ip, INET6_ADDRSTRLEN);
	return ip;
}

static
int get_gateway(int fd, struct sockaddr_nl *sa, int domain)
{
	char buf[BUFLEN];

	memset(buf, 0, BUFLEN);

	// assemble the message according to the netlink protocol
	struct nlmsghdr *nl;
	nl = (struct nlmsghdr*)buf;
	nl->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nl->nlmsg_type = RTM_GETROUTE;
	nl->nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;

	struct ifaddrmsg *ifa;
	ifa = (struct ifaddrmsg*)NLMSG_DATA(nl);
	ifa->ifa_family = domain; // we only get ipv4 address here

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
int parse_rt_msg(struct rtmsg *rt, void *buf, size_t len)
{
	char ifname[IF_NAMESIZE];
	printf("==================================\n");
	printf("family:\t\t%d\n", (rt->rtm_family == AF_INET) ? 4 :6);
	printf("table id:\t%d\n", rt->rtm_table);
	printf("protocol:\t%d\n", rt->rtm_protocol);
	printf("scope:\t\t%d\n", rt->rtm_scope);
	printf("type:\t\t%d\n", rt->rtm_type);
	printf("\n");

	struct rtattr *rta = NULL;
	int fa = rt->rtm_family;
	for_each_rattr(rta, buf, len) {
		if (rta->rta_type == RTA_GATEWAY) {
			printf("gateway:\t%s\n", ntop(fa, RTA_DATA(rta)));
		}
	}

	return 0;
}

static
uint32_t parse_nl_msg(void *buf, size_t len)
{
	struct nlmsghdr *nl = NULL;
	for_each_nlmsg(nl, buf, len) {
		if (nl->nlmsg_type == NLMSG_ERROR) {
			printf("error");
			return -1;
		}

		if (nl->nlmsg_type == RTM_NEWROUTE) {
			struct rtmsg *rt;
			rt = (struct rtmsg*)NLMSG_DATA(nl);
			parse_rt_msg(rt, RTM_RTA(rt), RTM_PAYLOAD(nl));
			continue;
		}


	}
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

	len = get_gateway(fd, &sa, AF_INET);
	check(len);

	char buf[BUFLEN];
	uint32_t nl_msg_type;
	do {
		len = get_msg(fd, &sa, buf, BUFLEN);
		check(len);

		nl_msg_type = parse_nl_msg(buf, len);
	} while (nl_msg_type != NLMSG_DONE && nl_msg_type != NLMSG_ERROR);

	return 0;
}
