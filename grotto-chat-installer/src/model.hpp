#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace grotto::installer {

enum class TlsMode {
    LetsEncrypt = 0,
    SelfSigned = 1,
    ExistingCert = 2,
};

struct ArtifactInfo {
    std::string url;
    std::string sha256;
};

struct PackInfo {
    std::string id;
    std::string name;
    std::string description;
    bool required = false;
};

struct Manifest {
    std::string version;
    std::string docs_url;
    ArtifactInfo installer;
    std::vector<PackInfo> packs;
    std::map<std::string, ArtifactInfo> artifacts;
};

struct SystemInfo {
    bool is_linux = false;
    bool is_root = false;
    bool has_systemd = false;
    bool has_ufw = false;
    bool has_apt = false;
    std::string distro_id;
    std::string distro_version;
    std::string arch;
    std::string platform_key;
};

struct InstallConfig {
    std::string domain = "grotto.local";
    std::string install_path = "/opt/grotto";
    std::string data_path = "/var/lib/grotto";
    std::string listen_address = "0.0.0.0";
    std::string port_text = "6697";
    TlsMode tls_mode = TlsMode::LetsEncrypt;
    std::string existing_cert_path = "/etc/grotto/cert.pem";
    std::string existing_key_path = "/etc/grotto/key.pem";
    bool manage_firewall = true;
    bool install_ufw_if_missing = true;
    bool start_after_install = true;
    std::vector<std::string> selected_pack_ids = {"server"};
};

struct PreflightIssue {
    bool blocking = false;
    std::string message;
};

struct InstallSummary {
    bool success = false;
    std::string headline;
    std::string config_path;
    std::string docs_url;
    std::vector<std::string> details;
    std::vector<std::string> manual_firewall_ports;
};

}  // namespace grotto::installer
