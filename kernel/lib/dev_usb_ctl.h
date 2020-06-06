#pragma once
#include "types.h"
#include "device/iocp.h"
#include "device/usb.h"

class usb_bus_t;

struct usb_iocp_result_t {
    usb_iocp_result_t() = default;

    operator bool() const
    {
        return cc == usb_cc_t::success;
    }

    int len_or_error()
    {
        return cc == usb_cc_t::success || cc == usb_cc_t::short_pkt
                ? ccp
                : -int(cc);
    }

    int slot_or_error()
    {
        return cc == usb_cc_t::success ? slotid : -int(cc);
    }

    uint32_t ccp = 0;
    uint32_t xfer_len = 0;
    usb_cc_t cc = usb_cc_t::invalid;
    uint8_t slotid = 0;

    static bool succeeded(usb_iocp_result_t const& status)
    {
        return status.cc == usb_cc_t::success;
    }
};

using usb_iocp_t = dgos::basic_iocp_t<usb_iocp_result_t>;
using usb_blocking_iocp_t = dgos::basic_blocking_iocp_t<usb_iocp_result_t>;

class usb_pipe_t {
public:
    usb_pipe_t();

    usb_pipe_t(usb_bus_t *bus, int slotid, int epid)
        : bus(bus)
        , slotid(slotid)
        , epid(epid)
    {
    }

    operator bool() const
    {
        return bus && slotid > 0 && epid >= 0;
    }

    int send_default_control(uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             uint16_t length, void *data) const;

    int send_default_control_async(uint8_t request_type, uint8_t request,
                                   uint16_t value, uint16_t index,
                                   uint16_t length, void *data,
                                   usb_iocp_t *iocp) const;

    int recv(void *data, uint32_t length) const;
    int recv_async(void *data, uint32_t length, usb_iocp_t *iocp) const;

    int send(void const *data, uint32_t length) const;
    int send_async(void const *data, uint32_t length, usb_iocp_t *iocp) const;

    int clear_ep_halt(usb_pipe_t const& target);

    bool add_hub_port(int port);

    bool set_hub_port_config(usb_hub_desc const& hub_desc,
                             usb_config_helper const *cfg_hlp);

private:
    usb_bus_t *bus;
    int slotid;
    int epid;
};

// Interface to a host controller
class usb_bus_t {
public:
    virtual ~usb_bus_t() {}

    // Reset the host controller
    virtual bool reset() { return false; }

    // Get the highest port number supported by this device
    virtual int get_max_ports() = 0;

    // Get port current connect status
    virtual bool port_device_present(size_t port) = 0;

    // Returns slot number on success, or negated completion code on error
    virtual int enable_slot(int port) = 0;

    // Returns 0 on success, or negated completion code on error
    virtual int set_address(int slotid, int port, uint32_t route) = 0;

    virtual bool get_pipe(int slotid, int epid, usb_pipe_t &pipe) = 0;

    virtual bool alloc_pipe(int slotid, usb_pipe_t &pipe,
                            int epid, int cfg_value,
                            int iface_num, int alt_iface,
                            int max_packet_sz, int interval,
                            usb_ep_attr ep_type) = 0;

    bool alloc_pipe(int slotid, usb_desc_iface const *iface,
                    usb_desc_ep const* ep, usb_pipe_t &pipe);

    virtual int send_control(
            int slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data) = 0;

    virtual int send_control_async(
            int slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data,
            usb_iocp_t *iocp) = 0;

    virtual int xfer(int slotid, uint8_t epid, uint16_t stream_id,
                     uint32_t length, void *data, int dir) = 0;

    virtual int xfer_async(
            int slotid, uint8_t epid, uint16_t stream_id,
            uint32_t length, void *data, int dir,
            usb_iocp_t *iocp) = 0;

    virtual usb_ep_state_t get_ep_state(int slotid, uint8_t epid) = 0;

    virtual int reset_ep(int slotid, uint8_t epid) = 0;
    virtual int reset_ep_async(int slotid, uint8_t epid, usb_iocp_t *iocp) = 0;

    virtual bool configure_hub_port(int slotid, int port) = 0;

    virtual bool set_hub_port_count(
            int slotid, usb_hub_desc const& hub_desc) = 0;
};

class usb_class_drv_t {
public:
    static usb_class_drv_t *find_driver(usb_config_helper *cfg, usb_bus_t *bus);

    usb_class_drv_t();
    virtual bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) = 0;

    virtual char const *name() const = 0;

protected:
    struct match_result {
        usb_desc_device const* dev = nullptr;
        usb_desc_config const* cfg = nullptr;
        usb_desc_iface const* iface = nullptr;
        int cfg_idx = 0;
        int iface_idx = 0;
    };

    static match_result match_config(usb_config_helper *cfg_hlp, size_t index,
            int dev_class, int dev_subclass, int dev_proto, int iface_proto,
            int vendor_id, int product_id);

private:
    static usb_class_drv_t *first_driver;
    usb_class_drv_t *next_driver;
};
