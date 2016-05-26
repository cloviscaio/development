/*
 * Copyright (C) 2015 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <utility>

#include <l4/cxx/ipc_server>
#include <l4/cxx/ref_ptr>
#include <l4/sys/irq>

#include "device.h"
#include "device_tree.h"

namespace Gic {

/**
 * An interrupt-emitting device.
 *
 * This is the generic interface for notifications from the
 * interrupt controller to an interrupt-emitting device.
 */
struct Irq_source : public virtual Vdev::Dev_ref
{
  virtual void eoi() = 0;
  virtual ~Irq_source() = 0;
};


/**
 * Generic interrupt controller interface.
 */
struct Ic : public Vdev::Device
{
  virtual ~Ic() = 0;
  virtual void set(unsigned irq) = 0;
  virtual void clear(unsigned irq) = 0;

  /**
   * Register a device source for forwarding downstream events.
   *
   * \param irq  Irq source to connect the listener to.
   * \param src  Device source. Replaces any currently attached
   *             listener. An empty pointer detaches the current source.
   */
  virtual void bind_irq_source(unsigned irq, cxx::Ref_ptr<Irq_source> src) = 0;

  /**
   * Determines the number of interrupts required for a device node
   * in the device tree.
   */
  virtual int dt_get_num_interrupts(Vdev::Dt_node const &node) = 0;

  /**
   * Get the id of the nth interrupt for a device node in the device tree.
   *
   * \return The GIC-internal id of the interrupt the device should be
   *         connected to. Use this to create the Irq_sink.
   */
  virtual unsigned dt_get_interrupt(Vdev::Dt_node const &node, int irq) = 0;

};

inline Ic::~Ic() = default;
inline Irq_source::~Irq_source() = default;

} // namespace

namespace Vmm {

/**
 * Generic interrupt line on an interrupt controller.
 *
 * The Irq_sink implements a line-triggered interrupt and
 * remembers it's current state. It will only notify the
 * interrupt controller when it's state changes, thus effectively
 * ignoring multiple inject() or ack().
 */
class Irq_sink
{
public:
  Irq_sink() : _ic(nullptr) {}

  Irq_sink(Gic::Ic *ic, unsigned irq)
  : _irq(irq), _ic(ic), _state(false)
  {}

  Irq_sink(Irq_sink const &) = delete;
  Irq_sink(Irq_sink &&other) = delete;

  ~Irq_sink()
  { ack(); }

  void rebind(Gic::Ic *ic, unsigned irq)
  {
    ack();

    _ic = ic;
    _irq = irq;
  }

  void inject()
  {
    if (_state || !_ic)
      return;

    _ic->set(_irq);
    _state = true;
  }

  void ack()
  {
    if (!_state || !_ic)
      return;

    _ic->clear(_irq);
    _state = false;
  }

private:
  unsigned _irq;
  Gic::Ic *_ic;
  bool _state;
};

} // namespace

