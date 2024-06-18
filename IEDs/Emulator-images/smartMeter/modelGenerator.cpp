#include <iostream>
#include <pugixml.hpp>

#define XML_FILE_TEMPLATE "template.cid"

#define XML_FILE "smartMeter.cid"

void update_ied_name(pugi::xml_document& doc, const char* ied_name) {
    // Find and update Header id
    pugi::xml_node header_node = doc.select_node("/SCL/Header").node();
    if (header_node) {
        header_node.attribute("id").set_value(ied_name);
    } else {
        std::cerr << "Failed to find Header node." << std::endl;
    }

    // Find and update iedName under ConnectedAP
    pugi::xml_node connectedap_node = doc.select_node("/SCL/Communication/SubNetwork/ConnectedAP").node();
    if (connectedap_node) {
        connectedap_node.attribute("iedName").set_value(ied_name);
    } else {
        std::cerr << "Failed to find ConnectedAP node." << std::endl;
    }

    // Find and update IED name
    pugi::xml_node ied_node = doc.select_node("/SCL/IED").node();
    if (ied_node) {
        ied_node.attribute("name").set_value(ied_name);
    } else {
        std::cerr << "Failed to find IED node." << std::endl;
    }
}

void update_ied_ip(pugi::xml_document& doc, const char* ip, const char* gateway) {
    // Find and update IP address
    pugi::xml_node ip_node = doc.select_node("/SCL/Communication/SubNetwork/ConnectedAP/Address/P[@type='IP']").node();

    if (ip_node) {
        ip_node.text().set(ip);
    } else {
        std::cerr << "Failed to find IP node." << std::endl;
    }

    // Find and update Gateway address
    pugi::xml_node gateway_node = doc.select_node("/SCL/Communication/SubNetwork/ConnectedAP/Address/P[@type='IP-GATEWAY']").node();
    if (gateway_node) {
        gateway_node.text().set(gateway);
    } else {
        std::cerr << "Failed to find Gateway node." << std::endl;
    }
}

void update_ied_mac(pugi::xml_document& doc, const char* mac_address) {
    // Find and update MAC address
    pugi::xml_node mac_node = doc.select_node("/SCL/Communication/SubNetwork/ConnectedAP/GSE/Address/P[@type='MAC-Address']").node();

    if (mac_node) {
        mac_node.text().set(mac_address);
    } else {
        std::cerr << "Failed to find MAC node." << std::endl;
    }
}

void update_ied_appId(pugi::xml_document& doc, int id) {
    // Find and update MAC address
    pugi::xml_node mac_node = doc.select_node("/SCL/Communication/SubNetwork/ConnectedAP/GSE/Address/P[@type='APPID']").node();

    if (mac_node) {
        mac_node.text().set(id);
    } else {
        std::cerr << "Failed to find AppId node." << std::endl;
    }
}

void update_sMonitoringVar(pugi::xml_document& doc, const char* ied_name, const char* var) {

    char path[100] = "";
    snprintf(path, 100, "/SCL/DataTypeTemplates/DOType[@id='%s']", var);

    // Find and update sMonitoringVar value
    pugi::xml_node spc_st_node = doc.select_node(path).node();
    if (spc_st_node) {
        pugi::xml_node st_val_node = spc_st_node.select_node("./DA[@name='mag']").node();
        if (st_val_node) {
            pugi::xml_node property_node = st_val_node.select_node("./Private/Private/Property[@Name='sMonitoringVar']").node();
            if (property_node) {
                std::string sMonitoringVar = property_node.attribute("Value").value();
                const std::string newSMonitoringVar = ied_name + sMonitoringVar;
                property_node.attribute("Value").set_value(newSMonitoringVar.c_str());
            } else {
                std::cerr << "Failed to find Property node." << std::endl;
            }
        } else {
            pugi::xml_node st_val_node = spc_st_node.select_node("./DA[@name='stVal']").node();
            if (st_val_node) {
                pugi::xml_node property_node = st_val_node.select_node("./Private/Property[@Name='sMonitoringVar']").node();
                if (property_node) {
                    std::string sMonitoringVar = property_node.attribute("Value").value();
                    const std::string newSMonitoringVar = ied_name + sMonitoringVar;
                    property_node.attribute("Value").set_value(newSMonitoringVar.c_str());
                } else {
                    std::cerr << "Failed to find Property node." << std::endl;
                }
            } else {
                std::cerr << "Failed to find node." << std::endl;
            }
        }
    } else {
        std::cerr << "Failed to find " << var << " node." << std::endl;
    }
}

void save_changes(const char* xml_file, const pugi::xml_document& doc) {
    // Save changes back to the file
    if (!doc.save_file(xml_file)) {
        std::cerr << "Failed to save changes to XML file." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <sm_id> <ip> <gateway>" << std::endl;
        return 1;
    }

    int sm_id = atoi(argv[1]);
    const char* ip = argv[2];
    const char* gateway = argv[3];

    // Load template file

    pugi::xml_document doc;
    if (!doc.load_file(XML_FILE_TEMPLATE)) {
        std::cerr << "Failed to load XML file: " << XML_FILE_TEMPLATE << std::endl;
        return 1;
    }

    char ied_name[100] = "";
    snprintf(ied_name, 100, "SM%02d", sm_id);

    char mac_address[18] = "";
    snprintf(mac_address, 18, "01-0C-CD-01-00-%02d", sm_id);

    update_ied_name(doc, ied_name);
    update_ied_ip(doc, ip, gateway);
    update_ied_mac(doc, mac_address);
    update_ied_appId(doc, sm_id);

    update_sMonitoringVar(doc, ied_name, "MV_AnVA");
    update_sMonitoringVar(doc, ied_name, "MV_AnVB");
    update_sMonitoringVar(doc, ied_name, "MV_AnVC");
    update_sMonitoringVar(doc, ied_name, "MV_AnIA");
    update_sMonitoringVar(doc, ied_name, "MV_AnIB");
    update_sMonitoringVar(doc, ied_name, "MV_AnIC");
    update_sMonitoringVar(doc, ied_name, "SPC_SEC");

    save_changes(XML_FILE, doc);

    return 0;
}
