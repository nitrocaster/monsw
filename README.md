# monsw
Lid driven monitor switch

monsw is a Windows service which turns off all displays when you close the lid of your laptop.
This is done by temporarily changing video powerdown timeout parameter in the current power plan to the minimum valid value (1 second).

## Usage

Run all these commands as administrator (remember, it's a service).

Installation:

    monsw -install

Running:

    net start monsw

Uninstallation:

    monsw -uninstall

## Requirements

- Windows Vista/Server 2008 or later
