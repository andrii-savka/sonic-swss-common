#include <string.h>
#include <errno.h>
#include <system_error>
#include "common/logger.h"
#include "common/netmsg.h"
#include "common/netdispatcher.h"
#include "common/netlink.h"

using namespace swss;
using namespace std;

NetLink::NetLink() :
    m_socket(NULL)
{
    m_socket = nl_socket_alloc();
    if (!m_socket)
    {
        SWSS_LOG_ERROR("Unable to allocated netlink socket");
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to allocated netlink socket");
    }

    nl_socket_disable_seq_check(m_socket);
    nl_socket_modify_cb(m_socket, NL_CB_VALID, NL_CB_CUSTOM, onNetlinkMsg, this);

    int err = nl_connect(m_socket, NETLINK_ROUTE);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to connect netlink socket: %s", nl_geterror(err));
        nl_socket_free(m_socket);
        m_socket = NULL;
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect netlink socket");
    }

    /* Set socket buffer size to 256KB */
    nl_socket_set_buffer_size(m_socket, 2097152, 0);
}

NetLink::~NetLink()
{
    if (m_socket != NULL)
    {
        nl_close(m_socket);
        nl_socket_free(m_socket);
    }
}

void NetLink::registerGroup(int rtnlGroup)
{
    int err = nl_socket_add_membership(m_socket, rtnlGroup);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to register to group %d: %s", rtnlGroup,
                       nl_geterror(err));
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to register group");
    }
}

void NetLink::dumpRequest(int rtmGetCommand)
{
    int err = nl_rtgen_request(m_socket, rtmGetCommand, AF_UNSPEC, NLM_F_DUMP);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to request dump on group %d: %s", rtmGetCommand,
                       nl_geterror(err));
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to request dump");
    }
}

void NetLink::addFd(fd_set *fd)
{
    FD_SET(nl_socket_get_fd(m_socket), fd);
}

bool NetLink::isMe(fd_set *fd)
{
    return FD_ISSET(nl_socket_get_fd(m_socket), fd);
}

int NetLink::readCache()
{
    return NODATA;
}

void NetLink::readMe()
{
    int err = nl_recvmsgs_default(m_socket);
    if (err < 0)
    {
        if (err == -NLE_NOMEM)
            SWSS_LOG_ERROR("netlink reports out of memory on reading a netlink socket. High possiblity of a lost message");
        else
            SWSS_LOG_ERROR("netlink reports an error=%d on reading a netlink socket", err);
    }
}

int NetLink::onNetlinkMsg(struct nl_msg *msg, void *arg)
{
    NetDispatcher::getInstance().onNetlinkMessage(msg);
    return NL_OK;
}
