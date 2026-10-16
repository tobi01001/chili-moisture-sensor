##############################################
# 99_myUtils_moisture.pm - receiver for the moisture sensor
# https://github.com/tobi01001/chili-moisture-sensor  (see docs/FHEM.md)
#
# Called by the sensor through a FHEM telnet port:
#   {handleMoistureSensor("Moisture_12345", "moi 42: raw 2100: wtr 0")}
#
# Unknown devices are created automatically as dummy devices.
package main;
use strict;
use warnings;

sub myUtils_moisture_Initialize { my $hash = shift; return; }

# Sensor keys (3 letters, identical for WiFi and the BLE gateway) -> reading names.
my %moisture_keys = (
	moi => 'moisture',  raw => 'raw_mv',      sup => 'supply_mv',
	usb => 'usb',       wtr => 'water',       ago => 'watered_ago_h',
	fwv => 'fw_version', mac => 'mac_address', int => 'interval_min', hbt => 'heartbeat_min',
);
# Readings that are updated (with event) on every message. The sensor only sends on
# change or heartbeat, so every message also refreshes the reading timestamp
# ("sensor still alive").
my $moisture_measured = qr/^(moisture|raw_mv|supply_mv|usb|water|watered_ago_h)$/;
# Static information, only sent once after power-on; updated on change only.
my $moisture_info = qr/^(fw_version|mac_address|interval_min|heartbeat_min)$/;

sub handleMoistureSensor($$)
{
	return if(@_ != 2);
	my ($device, $param) = @_;

	# Only accept the names the sensor generates (protects against typos/garbage).
	return if(!defined $device || $device !~ /^Moisture_\d{1,5}$/);
	return if(!defined $param);

	if(!IsDevice($device))
	{
		Log(2, "handleMoistureSensor: $device is not defined. Creating dummy.");
		fhem("defmod $device dummy");
		fhem("attr $device room MoistureSensors");
		fhem("attr $device stateFormat M: moisture % V: supply_mv mV");
		fhem("save");
	}

	my $hash = $defs{$device};
	readingsBeginUpdate($hash);
	foreach my $pair (split(':', $param))
	{
		$pair =~ s/^\s+|\s+$//g;
		my ($key, $value) = split(' ', $pair, 2);
		next if(!defined $key || !defined $value);
		my $reading = makeReadingName($moisture_keys{$key} // $key);  # long names still accepted

		if($reading =~ $moisture_measured && $value =~ /^-?\d+(\.\d+)?$/)
		{
			readingsBulkUpdate($hash, $reading, $value, 1);
			readingsBulkUpdate($hash, "state", $value ? "water" : "ok", 1) if($reading eq "water");
		}
		elsif($reading =~ $moisture_info && $value =~ /^[\w.\-]+$/)
		{
			readingsBulkUpdateIfChanged($hash, $reading, $value);
		}
		else
		{
			Log(3, "handleMoistureSensor: $device ignoring '$reading' = '$value'");
		}
	}
	readingsEndUpdate($hash, 1);
}

1;
