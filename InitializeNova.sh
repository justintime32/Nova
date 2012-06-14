#!/bin/bash

# Make sure we have root privs
if [[ $EUID -ne 0 ]]; then
   echo "You must be root to run this script. Aborting...";
   exit 1;
fi

# Add the user to the nova group
usermod -a -G nova $SUDO_USER

# Add /etc/sudoers.d to the sudoers file if needed
if grep -q "#includedir /etc/sudoers.d" /etc/sudoers; then
    echo "Note: sudoers.d is arleady included in /etc/sudoers"
else
    echo "No sudoers.d included in /etc/sudoers; attempting to add it";

    cp -f /etc/sudoers /tmp/sudoers.nova
    echo "#includedir /etc/sudoers.d" >> /tmp/sudoers.nova;

    if visudo -c -q -f /tmp/sudoers.nova; then
	echo "Saving changes to /etc/sudoers";
	cp -f /tmp/sudoers.nova /etc/sudoers;
    else
	echo "There was a problem with the sudoers file, aborting";
    fi

    rm -f /tmp/sudoers.nova
fi

QUERY="CREATE DATABASE nova_credentials; USE nova_credentials; CREATE TABLE credentials(user VARCHAR(100), pass VARCHAR(50)); INSERT INTO credentials VALUE('nova', SHA1('toor'));"

# /usr/bin/mysql --user=root --password=root <<< "DROP DATABASE nova_credentials"
/usr/bin/mysql --user=root --password=root <<< $QUERY

echo "A default Nova Web UI account has been created. It is recommended that you"
echo "delete the account after adding more users, as it doesn't possess a robust password"
