# monsw
Lid driven monitor switch

monsw is a Windows service which turns off all displays when you close the lid of your laptop.
This is done by temporarily changing video powerdown timeout parameter in the current power plan to minimum valid value (1 second).

## Usage

Installation (run as administrator):

    monsw -install

Running (run as administrator):

    net start monsw

Uninstallation (run as administrator):

    monsw -uninstall

## Requirements

- Windows Vista/Server 2008 or later
