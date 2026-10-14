##############################################
# 99_myUtils_chili.pm - receiver for the chili-moisture-sensor
# https://github.com/tobi01001/chili-moisture-sensor  (see docs/FHEM.md)
#
# Called by the sensor through a FHEM telnet port:
#   {handleChiliSensor("Chili_12345", "moisture 42: raw_mv 2100: water 0")}
#
# Unknown devices are created automatically as dummy devices.
package main;
use strict;
use warnings;

sub myUtils_chili_Initialize { my $hash = shift; return; }

# Readings that are updated (with event) on every message. The sensor only sends on
# change or heartbeat, so every message also refreshes the reading timestamp
# ("sensor still alive").
my $chili_measured = qr/^(moisture|raw_mv|supply_mv|usb|water|watered_ago_h)$/;
# Static information, only sent once after power-on; updated on change only.
my $chili_info = qr/^(fw_version|mac_address|interval_min|heartbeat_min)$/;

sub handleChiliSensor($$)
{
	return if(@_ != 2);
	my ($device, $param) = @_;

	# Only accept the names the sensor generates (protects against typos/garbage).
	return if(!defined $device || $device !~ /^Chili_\d{1,5}$/);
	return if(!defined $param);

	if(!IsDevice($device))
	{
		Log(2, "handleChiliSensor: $device is not defined. Creating dummy.");
		fhem("defmod $device dummy");
		fhem("attr $device room Chili");
		fhem("attr $device stateFormat M: moisture % V: supply_mv mV");
		fhem("save");
	}

	my $hash = $defs{$device};
	readingsBeginUpdate($hash);
	foreach my $pair (split(':', $param))
	{
		$pair =~ s/^\s+|\s+$//g;
		my ($reading, $value) = split(' ', $pair, 2);
		next if(!defined $reading || !defined $value);
		$reading = makeReadingName($reading);

		if($reading =~ $chili_measured && $value =~ /^-?\d+(\.\d+)?$/)
		{
			readingsBulkUpdate($hash, $reading, $value, 1);
			readingsBulkUpdate($hash, "state", $value ? "water" : "ok", 1) if($reading eq "water");
		}
		elsif($reading =~ $chili_info && $value =~ /^[\w.\-]+$/)
		{
			readingsBulkUpdateIfChanged($hash, $reading, $value);
		}
		else
		{
			Log(3, "handleChiliSensor: $device ignoring '$reading' = '$value'");
		}
	}
	readingsEndUpdate($hash, 1);
}

1;
