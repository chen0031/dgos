#include "dev_usb_ctl.h"

usb_class_drv_t *usb_class_drv_t::first_driver;

void usb_class_drv_t::find_driver(usb_config_helper *cfg, usb_bus_t *bus)
{
    for (usb_class_drv_t *drv = first_driver;
         drv && !drv->probe(cfg, bus); drv = drv->next_driver);
}

usb_class_drv_t::usb_class_drv_t()
{
    next_driver = first_driver;
    first_driver = this;
}


//        USBXHCI_TRACE("cfg #0x%x max_power=%umA {\n",
//                      cfg_idx, cfg->max_power * 2);
//            USBXHCI_TRACE("  iface #0x%x: class=0x%x (%s)"
//                          " subclass=0x%x, proto=0x%x {\n",
//                          iface->iface_num, iface->iface_class,
//                          cfg_hlp.class_code_text(iface->iface_class),
//                          iface->iface_subclass, iface->iface_proto);
//                USBXHCI_TRACE("    ep #0x%x: dir=%s addr=0x%x attr=%s,"
//                              " maxpktsz=0x%x, interval=%u\n",
//                              ep_idx, ep->ep_addr >= 0x80 ? "IN" : "OUT",
//                              ep->ep_addr,
//                              usb_config_helper::ep_attr_text(ep->ep_attr),
//                              ep->max_packet_sz, ep->interval);
//            USBXHCI_TRACE("  }\n");
//        USBXHCI_TRACE("}\n");
//    USBXHCI_TRACE("Done configuration descriptors\n");

usb_class_drv_t::match_result
usb_class_drv_t::match_config(usb_config_helper *cfg_hlp, int index,
                              int dev_class, int dev_subclass,
                              int vendor_id, int product_id)
{
    usb_desc_device const &dev_desc = cfg_hlp->device();

    match_result result{ &dev_desc, nullptr, nullptr, -1, -1 };

    int match_index = -1;

    if (vendor_id == dev_desc.vendor_id &&
        product_id == dev_desc.vendor_id) {
        if (++match_index == index)
            return result;
    }

    if (dev_class >= 0 || dev_subclass >= 0) {
        for (result.cfg_idx = 0;
             (result.cfg = cfg_hlp->find_config(result.cfg_idx)) != nullptr;
             ++result.cfg_idx) {
            for (result.iface_idx = 0;
                 (result.iface = cfg_hlp->find_iface(
                      result.cfg, result.iface_idx)) != nullptr;
                 ++result.iface_idx) {
                if ((dev_class < 0 ||
                     dev_class == result.iface->iface_class) &&
                        (dev_subclass < 0 ||
                         dev_subclass == result.iface->iface_subclass)) {
                    if (++match_index == index)
                        return result;
                }
            }
        }
    }

    result.dev = nullptr;
    result.cfg = nullptr;
    result.iface = nullptr;
    result.cfg_idx = -1;
    result.iface_idx = -1;

    return result;
}

int usb_pipe_t::send_default_control(uint8_t request_type, uint8_t request,
                                     uint16_t value, uint16_t index,
                                     uint16_t length, void *data)
{
//    printdbg("Sending USB control: reqt=0x%02x req=0x%02x, val=0x%04x,"
//             " idx=0x%02x, len=0x%04x, data=%p\n",
//             request_type, request, value, index, length, data);

    return bus->send_control(
                slotid, request_type, request, value, index, length, data);
}

int usb_pipe_t::recv(uint16_t length, void *data)
{
    return bus->xfer(slotid, epid, 0, length, data, 1);
}

int usb_pipe_t::send(uint16_t length, const void *data)
{
    return bus->xfer(slotid, epid, 0, length, const_cast<void*>(data), 0);
}
