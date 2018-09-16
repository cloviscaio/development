/*
 * (c) 2013-2014 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/*
 * Copyright (C) 2015-2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *            Jean Wolter <jean.wolter@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "debug.h"
#include "virtio_dev.h"
#include "virtio_event_connector.h"

#include <l4/re/error_helper>
#include <l4/re/event_enums.h>

namespace Vdev {

struct Virtio_input_event {
  // XXX little endian, we currently do not handle this for big endian
  // platforms
  l4_uint16_t type;
  l4_uint16_t code;
  l4_uint16_t value;
};

template <typename DEV>
class Virtio_input
: public Virtio::Dev
{
  typedef L4virtio::Svr::Virtqueue::Desc Desc;
  typedef L4virtio::Svr::Request_processor Request_processor;

  struct Payload
  {
    char *data;
    unsigned len;
    bool writable;
  };

  enum
  {
    Input_event_queue = 0,
    Input_status_queue = 1,
    Input_queue_num = 2,
    Input_queue_length = 0x100,
  };

protected:
  Virtio::Virtqueue _vqs[Input_queue_num];

public:
  // Based on BSD licenced include/uapi/linux/virtio_input.h
  enum Virtio_input_config_select {
    VIRTIO_INPUT_CFG_UNSET      = 0x00,
    VIRTIO_INPUT_CFG_ID_NAME    = 0x01,
    VIRTIO_INPUT_CFG_ID_SERIAL  = 0x02,
    VIRTIO_INPUT_CFG_ID_DEVIDS  = 0x03,
    VIRTIO_INPUT_CFG_PROP_BITS  = 0x10,
    VIRTIO_INPUT_CFG_EV_BITS    = 0x11,
    VIRTIO_INPUT_CFG_ABS_INFO   = 0x12,
  };

  struct Virtio_input_devids {
    l4_uint16_t bustype;
    l4_uint16_t vendor;
    l4_uint16_t product;
    l4_uint16_t version;
  };

  struct Virtio_input_absinfo {
    l4_uint32_t min;
    l4_uint32_t max;
    l4_uint32_t fuzz;
    l4_uint32_t flat;
    l4_uint32_t res;
  };

  struct Virtio_input_config {
    l4_uint8_t    select;
    l4_uint8_t    subsel;
    l4_uint8_t    size;
    l4_uint8_t    reserved[5];
    union {
      char string[128];
      l4_uint8_t bitmap[128];
      struct Virtio_input_devids ids;
      struct Virtio_input_absinfo;
    } u;
  };

  Virtio_input(Vmm::Vm_ram *iommu)
  : Virtio::Dev(iommu, 0x44, L4VIRTIO_ID_INPUT)
  {
    Features feat(0);
    feat.ring_indirect_desc() = true;
    _cfg_header->dev_features_map[0] = feat.raw;
    _cfg_header->num_queues = Input_queue_num;

    for (auto &q : _vqs)
      q.config.num_max = Input_queue_length;
  }

  void virtio_queue_ready(unsigned ready)
  {
    auto *q = current_virtqueue();
    if (!q)
      return;

    auto *qc = &q->config;

    if (ready == 0 && q->ready())
      {
        q->disable();
        qc->ready = 0;
      }
    else if (ready == 1 && !q->ready())
      {
        qc->ready = 0;
        l4_uint16_t num = qc->num;
        // num must be: a power of two in range [1,num_max].
        if (!num || (num & (num - 1)) || num > qc->num_max)
          return;

        q->init_queue(dev()->template devaddr_to_virt<void>(qc->desc_addr),
                      dev()->template devaddr_to_virt<void>(qc->avail_addr),
                      dev()->template devaddr_to_virt<void>(qc->used_addr));
        qc->ready = 1;
      }
  }

  void reset() override
  {
    for (auto &q : _vqs)
      {
        q.disable();
        q.config.num_max = Input_queue_length;
      }
  }

  void load_desc(Desc const &desc, Request_processor const *, Payload *p)
  {
    p->data = devaddr_to_virt<char>(desc.addr.get(), desc.len);
    p->len = desc.len;
    p->writable = desc.flags.write();
  }

  void load_desc(Desc const &desc, Request_processor const *,
                 Desc const **table)
  {
    *table = devaddr_to_virt<Desc const>(desc.addr.get(), sizeof(Desc));
  }

  void virtio_irq_ack(unsigned val)
  {
    _irq_status_shadow &= ~val;
    if (_cfg_header->irq_status != _irq_status_shadow)
      dev()->set_irq_status(_irq_status_shadow);

    dev()->event_connector()->clear_events(val);
  }

  Virtio::Virtqueue *virtqueue(unsigned qn) override
  { return qn < Input_queue_num ? &_vqs[qn] : nullptr; }

  int inject_events(Virtio_input_event *events, size_t num)
  {
    if (!num)
      return 0;

    unsigned injected;

    auto *q = &_vqs[0];
    if (!q->ready())
      {
        Err().printf("Virtio_input: not ready yet\n");
        return 0;
      }

    Request_processor rp;
    Payload p;

    for (injected = 0; injected < num; ++injected)
      {
        auto req = q->next_avail();

        if (!req)
          {
            Dbg(Dbg::Dev, Dbg::Warn, "virtio")
              .printf("Virtio_input: No request available\n");
            break;
          }

        rp.start(this, req, &p);

        // Check consistency of buffer
        if (!p.writable || p.len < sizeof(events[0]))
          {
            Dbg(Dbg::Dev, Dbg::Warn, "virtio")
              .printf("Virtio_input: buffer %s\n",
                      p.writable ? "read only" : "too small");
            // return it to the queue with 0 content
            q->consumed(req, 0);
            break;
          }

        memcpy(p.data, &events[injected], sizeof(events[0]));
        q->consumed(req, sizeof(events[0]));
      }

    // If we end up here we either successfully injected events or got an error
    // while doing so (e.g. no buffer available anymore). We notify the guest if
    // it did not disable notifications.
    // We could consider sending notifications in error cases even with disabled
    // notificatins (by adding  || (injected != num))but are currently not doing
    // so.
    if (!q->no_notify_guest())
      {
        dev()->_irq_status_shadow |= 1;
        if (_cfg_header->irq_status != _irq_status_shadow)
          dev()->set_irq_status(_irq_status_shadow);

        Virtio::Event_set ev;
        ev.set(q->config.driver_notify_index);
        dev()->event_connector()->send_events(cxx::move(ev));
      }

    return injected;
  }

private:
  DEV *dev() { return static_cast<DEV *>(this); }
};

}
