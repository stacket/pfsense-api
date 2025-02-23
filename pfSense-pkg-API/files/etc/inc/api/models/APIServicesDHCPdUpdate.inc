<?php
//   Copyright 2021 Jared Hendrickson
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

require_once("api/framework/APIModel.inc");
require_once("api/framework/APIResponse.inc");


class APIServicesDHCPdUpdate extends APIModel {
    # Create our method constructor
    public function __construct() {
        parent::__construct();
        $this->privileges = ["page-all", "page-services-dhcpserver"];
        $this->change_note = "Modified DHCP server via API";
    }

    public function action() {
        $this->config["dhcpd"][$this->id] = $this->validated_data;
        $this->write_config();
        $this->reload_dhcpd();
        return APIResponse\get(0, $this->validated_data);
    }

    public function validate_payload() {
        # Check for our required 'interface' payload value
        if (isset($this->initial_data["interface"])) {
            $this->initial_data["interface"] = APITools\get_pfsense_if_id($this->initial_data["interface"]);
            # Check that interface exists
            if (is_string($this->initial_data["interface"])) {
                $if_conf = $this->config["interfaces"][$this->initial_data["interface"]];
                # Check that interface hosts a static network with multiple addresses
                if (isset($if_conf["enable"]) and is_ipaddrv4($if_conf["ipaddr"]) and $if_conf["subnet"] <= 31) {
                    $this->id = $this->initial_data["interface"];
                    $this->validated_data = $this->config["dhcpd"][$this->id];
                } else {
                    $this->errors[] = APIResponse\get(2019);
                }
            } else {
                $this->errors[] = APIResponse\get(2018);
            }
        } else {
            $this->errors[] = APIResponse\get(2017);
        }

        # Check for our optional 'enabled' payload value
        if ($this->initial_data["enable"] === true) {
            $this->validated_data["enable"] = true;
        } elseif ($this->initial_data["enable"] === false) {
            $this->validated_data["enable"] = false;
        }

        # Format our range config arrays before validating their inputs
        if (!is_array($this->validated_data["range"])) {
            $this->config["range"] = [];
        }
        if (empty($this->initial_data["range_from"])) {
            $this->initial_data["range_from"] = $this->validated_data["range"]["from"];
        }
        if (empty($this->initial_data["range_to"])) {
            $this->initial_data["range_to"] = $this->validated_data["range"]["to"];
        }

        # Check for our optional 'range_from' payload value
        if (isset($this->initial_data["range_from"])) {
            # Ensure our range_from is an IPv4 address
            if (is_ipaddrv4($this->initial_data["range_from"])) {
                $ip = ip2long($this->initial_data["range_from"]);
                $gateway_ip = ip2long($if_conf["ipaddr"]);
                $start_ip = ip2long(gen_subnetv4($if_conf["ipaddr"], $if_conf["subnet"]));
                $end_ip = ip2long($this->initial_data["range_to"]);
                # Ensure our range_from is within our available range
                if ($ip !== $gateway_ip and $ip >= $start_ip and $ip <= $end_ip) {
                    $this->validated_data["range"]["from"] = $this->initial_data["range_from"];
                } else {
                    $this->errors[] = APIResponse\get(2022);
                }
            } else {
                $this->errors[] = APIResponse\get(2021);
            }
        } elseif (!isset($this->validated_data["range"]["from"])) {
            $this->errors[] = APIResponse\get(2020);
        }

        # Check for our optional 'range_to' payload value
        if (isset($this->initial_data["range_to"])) {
            # Ensure our range_to is an IPv4 address
            if (is_ipaddrv4($this->initial_data["range_to"])) {
                $ip = ip2long($this->initial_data["range_to"]);
                $gateway_ip = ip2long($if_conf["ipaddr"]);
                $start_ip = ip2long($this->validated_data["range_from"]);
                $end_ip = ip2long(gen_subnetv4_max($if_conf["ipaddr"], $if_conf["subnet"]));
                # Ensure our range_to is within our available range
                if ($ip !== $gateway_ip and $ip >= $start_ip and $ip <= $end_ip) {
                    $this->validated_data["range"]["to"] = $this->initial_data["range_to"];
                } else {
                    $this->errors[] = APIResponse\get(2025);
                }
            } else {
                $this->errors[] = APIResponse\get(2024);
            }
        } elseif (!isset($this->validated_data["range"]["to"])) {
            $this->errors[] = APIResponse\get(2023);
        }

        # Check for our optional 'domain' value
        if (isset($this->initial_data["domain"])) {
            # Ensure domain is a hostname or default
            if (is_hostname($this->initial_data["domain"]) or $this->initial_data["domain"] === "") {
                $this->validated_data["domain"] = $this->initial_data["domain"];
            } else {
                $this->errors[] = APIResponse\get(2030);
            }
        }

        # Check for our optional 'mac_allow' value
        if (isset($this->initial_data["mac_allow"])) {
            # Ensure value is an array
            if (!is_array($this->initial_data["mac_allow"])) {
                $this->initial_data["mac_allow"] = [$this->initial_data["mac_allow"]];
            }
            # Loop through each mac_allow value
            $macs = [];
            foreach ($this->initial_data["mac_allow"] as $mac) {
                # Ensure entry is partial mac or default
                if (is_macaddr($mac, true) or $mac === "") {
                    $macs[] = $mac;
                } else {
                    $this->errors[] = APIResponse\get(2032);
                }
            }

            # If we have validated values, format them to the XML supported format. Otherwise assume default.
            if (count($macs) > 0) {
                $this->validated_data["mac_allow"] = implode(",", $macs);
            } else {
                $this->validated_data["mac_allow"] = "";
            }
        }

        # Check for our optional 'mac_deny' value
        if (isset($this->initial_data["mac_deny"])) {
            # Ensure value is an array
            if (!is_array($this->initial_data["mac_deny"])) {
                $this->initial_data["mac_deny"] = [$this->initial_data["mac_deny"]];
            }
            # Loop through each mac_deny value
            $macs = [];
            foreach ($this->initial_data["mac_deny"] as $mac) {
                # Ensure entry is partial mac or default
                if (is_macaddr($mac, true) or $mac === "") {
                    $macs[] = $mac;
                } else {
                    $this->errors[] = APIResponse\get(2033);
                }
            }

            # If we have validated values, format them to the XML supported format. Otherwise assume default.
            if (count($macs) > 0) {
                $this->validated_data["mac_deny"] = implode(",", $macs);
            } else {
                $this->validated_data["mac_deny"] = "";
            }
        }

        # Check for our optional 'domainsearchlist' value
        if (isset($this->initial_data["domainsearchlist"])) {
            # Ensure value is an array
            if (!is_array($this->initial_data["domainsearchlist"])) {
                $this->initial_data["domainsearchlist"] = [$this->initial_data["domainsearchlist"]];
            }
            # Loop through each domainsearrchlist value
            $domains = [];
            foreach ($this->initial_data["domainsearchlist"] as $domain) {
                # Ensure domain is a hostname or default
                if (is_hostname($domain) or $domain === "") {
                    $domains[] = $domain;
                } else {
                    $this->errors[] = APIResponse\get(2031);
                }
            }

            # If we have validated values, format them to the XML supported format. Otherwise assume default.
            if (count($domains) > 0) {
                $this->validated_data["domainsearchlist"] = implode(";", $domains);
            } else {
                $this->validated_data["domainsearchlist"] = "";
            }
        }

        # Check for our optional 'gateway' payload value
        if (isset($this->initial_data["gateway"])) {
            # Ensure our gateway is an IPv4 address
            if (is_ipaddrv4($this->initial_data["gateway"])) {
                $ip = ip2long($this->initial_data["gateway"]);
                $start_ip = ip2long(gen_subnetv4($if_conf["ipaddr"], $if_conf["subnet"]));
                $end_ip = ip2long(gen_subnetv4_max($if_conf["ipaddr"], $if_conf["subnet"]));
                # Ensure our gateway is within our available range
                if ($ip > $start_ip and $ip < $end_ip) {
                    $this->validated_data["gateway"] = $this->initial_data["gateway"];
                } else {
                    $this->errors[] = APIResponse\get(2029);
                }
            # Allow empty string to revert gateway to default
            } elseif ($this->initial_data["gateway"] === "") {
                $this->validated_data["gateway"] = $this->initial_data["gateway"];
            } else {
                $this->errors[] = APIResponse\get(2028);
            }
        }

        # Check for our optional 'dnsserver' payload value
        if (isset($this->initial_data["dnsserver"])) {
            # Ensure this value is an array
            if (!is_array($this->initial_data["dnsserver"])) {
                $this->initial_data["dnsserver"] = [$this->initial_data["dnsserver"]];
            }

            # Ensure we have 4 or less DNS servers
            if (count($this->initial_data["dnsserver"]) <= 4) {
                $this->validated_data["dnsserver"] = [];
                # Loop through each requested DNS server and ensure it is valid
                foreach ($this->initial_data["dnsserver"] as $server) {
                    # Ensure value is IPv4 address
                    if (is_ipaddrv4($server)) {
                        $this->validated_data["dnsserver"][] = $server;
                    } else {
                        $this->errors[] = APIResponse\get(2027);
                    }
                }
            } else {
                $this->errors[] = APIResponse\get(2026);
            }
        }

        # Check for our optional 'ignorebootp' payload value
        if ($this->initial_data["ignorebootp"] === true) {
            $this->validated_data["ignorebootp"] = true;
        } elseif ($this->initial_data["ignorebootp"] === false) {
            $this->validated_data["ignorebootp"] = false;
        }

        # Check for our optional 'denyunknown' payload value
        if ($this->initial_data["denyunknown"] === true) {
            $this->validated_data["denyunknown"] = true;
        } elseif ($this->initial_data["denyunknown"] === false) {
            $this->validated_data["denyunknown"] = false;
        }
    }

    # Executes functions to reload DHCP and corresponding/dependent services
    protected function reload_dhcpd() {
        $retvaldhcp = 0;
        $retvaldns = 0;
        /* dnsmasq_configure calls dhcpd_configure */
        /* no need to restart dhcpd twice */
        if (isset($this->config['dnsmasq']['enable']) && isset($this->config['dnsmasq']['regdhcpstatic']))	{
            $retvaldns |= services_dnsmasq_configure();
            if ($retvaldns == 0) {
                clear_subsystem_dirty('hosts');
                clear_subsystem_dirty('staticmaps');
            }
        } else if (isset($this->config['unbound']['enable']) && isset($this->config['unbound']['regdhcpstatic'])) {
            $retvaldns |= services_unbound_configure();
            if ($retvaldns == 0) {
                clear_subsystem_dirty('unbound');
                clear_subsystem_dirty('hosts');
                clear_subsystem_dirty('staticmaps');
            }
        } else {
            $retvaldhcp |= services_dhcpd_configure();
            if ($retvaldhcp == 0) {
                clear_subsystem_dirty('staticmaps');
            }
        }
        /* BIND package - Bug #3710 */
        if (!function_exists('is_package_installed')) {
            require_once('pkg-utils.inc');
        }
        if (is_package_installed('pfSense-pkg-bind') && isset($this->config['installedpackages']['bind']['config'][0]['enable_bind'])) {
            $reloadbind = false;
            if (is_array($this->config['installedpackages']['bindzone'])) {
                $bindzone = $this->config['installedpackages']['bindzone']['config'];
            } else {
                $bindzone = array();
            }
            for ($x = 0; $x < sizeof($bindzone); $x++) {
                $zone = $bindzone[$x];
                if ($zone['regdhcpstatic'] == 'on') {
                    $reloadbind = true;
                    break;
                }
            }
            if ($reloadbind === true) {
                if (file_exists("/usr/local/pkg/bind.inc")) {
                    require_once("/usr/local/pkg/bind.inc");
                    bind_sync();
                }
            }
        }
        filter_configure();
    }
}
