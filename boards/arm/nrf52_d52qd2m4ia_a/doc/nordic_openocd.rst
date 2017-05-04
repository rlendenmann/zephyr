.. _nordic_openocd:

Nordic nRF5x Segger J-Link
##########################

Overview
********

All Nordic nRF5x Development Kits, Preview Development Kits and Dongles are equipped
with a Debug IC (Atmel ATSAM3U2C) which provides the following functionality:

* Segger J-Link firmware and desktop tools
* SWD debug for the nRF5x IC
* Mass Storage device for drag-and-drop image flashing
* USB CDC ACM Serial Port bridged to the nRF5x UART peripheral
* Segger RTT Console
* Segger Ozone Debugger

Segger J-Link Software Installation
***********************************

To install the J-Link Software and documentation pack, follow the steps below:

#. Download the appropriate package from the `J-Link Software and documentation pack`_ website
#. Depending on your platform, install the package or run the installer
#. When connecting a J-Link-enabled board such as an nRF5x DK, PDK or dongle, a
   drive corresponding to a USB Mass Storage device as well as a serial port should come up

nRF5x Command-Line Tools Installation
*************************************

The nRF5x command-line Tools allow you to control your nRF5x device from the command line,
including resetting it, erasing or programming the flash memory and more.

To install them, use the appropriate link for your operating system:

* `nRF5x Command-Line Tools for Windows`_
* `nRF5x Command-Line Tools for Linux 32-bit`_
* `nRF5x Command-Line Tools for Linux 64-bit`_
* `nRF5x Command-Line Tools for macOS`_

After installing, make sure that ``nrfjprog`` is somewhere in your executable path
to be able to invoke it from anywhere.

.. _nordic_openocd_flashing:

Flashing
********

To program the flash with a compiled Zephyr image after having followed the instructions
to install openOCD, follow the steps below:

* Connect the micro-USB cable to the nRF5x board and to your computer

* Erase and flash the Zephyr image from the sample folder of your choice:

.. code-block:: console

   $ openocd -f board/nordic_nrf52_ftx232.cfg -c "program outdir/<board>/zephyr.hex verify reset exit"

Where: ``<board>`` is the board name you used in the BOARD directive when building (for example d52qd2m4ia_a).
Above openocd command erases, flashes and reset the device.

* Reset and start Zephyr:

.. code-block:: console

   $ openocd -f board/nordic_nrf52_ftx232.cfg -c "reset exit"

USB CDC ACM Serial Port Setup
*****************************

**Important note**: An issue with Segger J-Link firmware on the nRF5x boards might cause
data loss and/or corruption on the USB CDC ACM Serial Port on some machines.

Windows
=======

The serial port will appear as ``COMxx``. Simply check the "Ports (COM & LPT)" section
in the Device Manager.

GNU/Linux
=========

The serial port will appear as ``/dev/ttyACMx``. By default the port is not accessible to all users.
Type the command below to add your user to the dialout group to give it access to the serial port.
Note that re-login is required for this to take effect.

.. code-block:: console

   $ sudo usermod -a -G dialout <username>

To avoid it being taken by the Modem Manager for a few seconds when you plug the board in:

.. code-block:: console

   systemctl stop ModemManager.service
   systemctl disable ModemManager.service

Apple macOS (OS X)
==================

The serial port will appear as ``/dev/tty.usbmodemXXXX``.


.. _nRF52 DK website: http://www.nordicsemi.com/eng/Products/Bluetooth-Smart-Bluetooth-low-energy/nRF52-DK
.. _Nordic Semiconductor Infocenter: http://infocenter.nordicsemi.com/
.. _J-Link Software and documentation pack: https://www.segger.com/jlink-software.html
