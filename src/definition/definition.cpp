#include "definition/definition.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace sadhana {

void RitualDefinition::loadMaterialsFromJson(const JsonValue& json) {
    std::cout << "Loading materials from JSON..." << std::endl;
    std::cout << "JSON content type: " << json.type_name() << std::endl;

    materials_.clear();

    const JsonValue* materialsArray = nullptr;
    
    if (json.contains("materials") && json["materials"].is_array()) {
        materialsArray = &json["materials"];
    } else if (json.is_array()) {
        materialsArray = &json;
    }

    if (!materialsArray) {
        std::cerr << "Warning: No materials array found in expected locations" << std::endl;
        return;
    }

    for (const auto& materialJson : *materialsArray) {
        Material material;
        
        if (!materialJson.contains("id") || !materialJson.contains("name")) {
            std::cerr << "Skipping material: missing required fields (id or name)" << std::endl;
            continue;
        }

        material.id = materialJson["id"].get<std::string>();
        material.name = materialJson["name"].get<std::string>();
        
        if (materialJson.contains("details")) {
            material.details = materialJson["details"].get<std::string>();
        }
        if (materialJson.contains("optional")) {
            material.optional = materialJson["optional"].get<bool>();
        }

        for (const auto& [key, value] : materialJson.items()) {
            if (key != "id" && key != "name" && key != "details" && key != "optional") {
                material.additional_data[key] = value;
            }
        }

        materials_.push_back(std::move(material));
    }

    std::cout << "Successfully loaded " << materials_.size() << " materials" << std::endl;
}

bool RitualDefinition::loadFromFile(const std::string& filepath) {
    try {
        std::cout << "Opening file: " << filepath << std::endl;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return false;
        }

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }

        JsonValue mainJson = nlohmann::json::parse(content);
        if (!loadFromJson(mainJson)) {
            return false;
        }

        std::filesystem::path absPath = std::filesystem::absolute(filepath);
        std::filesystem::path basePath = absPath.parent_path().parent_path().parent_path();

        if (mainJson.contains("materials_ref")) {
            auto materialsPath = basePath / "common" / "materials" /
                               (mainJson["materials_ref"].get<std::string>() + ".json");
            std::cout << "Loading materials from: " << materialsPath << std::endl;

            if (std::filesystem::exists(materialsPath)) {
                std::ifstream materialsFile(materialsPath);
                std::string materialsContent((std::istreambuf_iterator<char>(materialsFile)),
                                          std::istreambuf_iterator<char>());
                std::cout << "Materials file size: " << materialsContent.length() << " bytes" << std::endl;

                JsonValue materialsJson = nlohmann::json::parse(materialsContent);
                loadMaterialsFromJson(materialsJson);
            }
        }

        if (mainJson.contains("mantras_ref")) {
            auto mantrasPath = basePath / "common" / "mantras" /
                              (mainJson["mantras_ref"].get<std::string>() + ".json");
            
            std::cout << "Debug: Base path: " << basePath << std::endl;
            std::cout << "Debug: Mantras ref: " << mainJson["mantras_ref"].get<std::string>() << std::endl;
            std::cout << "Debug: Full mantras path: " << mantrasPath << std::endl;
            
            std::ifstream testFile(mantrasPath);
            if (testFile.is_open()) {
                std::string rawContent;
                testFile.seekg(0, std::ios::end);
                rawContent.reserve(testFile.tellg());
                testFile.seekg(0, std::ios::beg);
                rawContent.assign((std::istreambuf_iterator<char>(testFile)),
                                std::istreambuf_iterator<char>());
                std::cout << "Debug: Raw file content first 100 chars:\n" << rawContent.substr(0, 100) << std::endl;
                testFile.close();
            } else {
                std::cerr << "Debug: Could not open mantras file for direct reading" << std::endl;
            }

            std::cout << "Loading mantras from: " << mantrasPath << std::endl;

            if (std::filesystem::exists(mantrasPath)) {
                std::ifstream mantrasFile(mantrasPath);
                std::string mantrasContent((std::istreambuf_iterator<char>(mantrasFile)),
                                        std::istreambuf_iterator<char>());
                std::cout << "Mantras file content length: " << mantrasContent.length() << " bytes" << std::endl;

                try {
                    JsonValue mantrasJson = nlohmann::json::parse(mantrasContent);
                    loadMantrasFromJson(mantrasJson);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing mantras file: " << e.what() << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Mantras file not found at: " << mantrasPath << std::endl;
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading ritual definition: " << e.what() << std::endl;
        return false;
    }
}

bool RitualDefinition::loadFromJson(const JsonValue& json) {
    try {
        parseFromJson(json);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing ritual definition: " << e.what() << std::endl;
        return false;
    }
}

void RitualDefinition::loadProceduresFromJson(const JsonValue& json) {
    if (!json.contains("procedures")) return;

    for (const auto& [id, proc] : json["procedures"].items()) {
        if (proc.contains("steps")) {
            auto section_it = std::find_if(sections_.begin(), sections_.end(),
                [&id](const Section& s) { return s.id == id; });

            if (section_it != sections_.end()) {
                std::vector<Step> steps;
                for (const auto& step_json : proc["steps"]) {
                    Step step;
                    step.id = step_json.at("id").get<std::string>();
                    step.title = step_json.at("title").get<std::string>();
                    step.items = step_json.value("items", std::vector<std::string>());
                    step.instructions = step_json.value("instructions", std::vector<std::string>());
                    step.mantra_refs = step_json.value("mantra_refs", std::vector<std::string>());

                    for (const auto& [key, value] : step_json.items()) {
                        if (key != "id" && key != "title" && key != "items" &&
                            key != "instructions" && key != "mantra_refs") {
                            step.additional_data[key] = value;
                        }
                    }

                    steps.push_back(std::move(step));
                }
                section_it->steps = std::move(steps);
            }
        }
    }
}

void RitualDefinition::parseFromJson(const JsonValue& json) {
    id_ = json.at("id").get<std::string>();
    title_ = json.at("title").get<std::string>();
    version_ = json.at("version").get<std::string>();
    source_ = json.at("source").get<std::string>();

    if (json.contains("metadata")) {
        metadata_ = json["metadata"].get<MetadataMap>();
    }

    if (json.contains("sections")) {
        for (const auto& section_json : json["sections"]) {
            Section section;
            section.id = section_json.at("id").get<std::string>();
            section.title = section_json.at("title").get<std::string>();
            section.notes = section_json.value("discipline_note", "");

            if (section_json.contains("iteration_marker")) {
                const auto& marker_json = section_json["iteration_marker"];
                ProgressMarker marker;
                marker.canonical = marker_json.at("canonical").get<std::string>();
                marker.variants = marker_json.value("variants", std::vector<std::string>());
                marker.with_svaha_variants = marker_json.value("with_svaha_variants", false);
                marker.cooldown_ms = marker_json.value("cooldown_ms", 700);

                for (const auto& [key, value] : marker_json.items()) {
                    if (key != "canonical" && key != "variants" &&
                        key != "with_svaha_variants" && key != "cooldown_ms") {
                        marker.additional_params[key] = value;
                    }
                }

                section.iteration_marker = std::move(marker);
            }

            if (section_json.contains("parts")) {
                std::vector<Part> parts;
                for (const auto& part_json : section_json["parts"]) {
                    Part part;
                    part.id = part_json.at("id").get<std::string>();
                    part.title = part_json.at("title").get<std::string>();
                    part.notes = part_json.value("notes", "");

                    if (part_json.contains("repetitions")) {
                        part.repetitions = part_json["repetitions"].get<int>();
                    }
                    if (part_json.contains("utterance")) {
                        part.utterance = part_json["utterance"].get<std::string>();
                    }
                    if (part_json.contains("sequence")) {
                        part.sequence = part_json["sequence"].get<std::vector<std::string>>();
                    }
                    if (part_json.contains("pairs")) {
                        part.pairs = part_json["pairs"].get<std::vector<std::vector<std::string>>>();
                    }

                    if (part_json.contains("derived_counts")) {
                        for (const auto& [key, value] : part_json["derived_counts"].items()) {
                            part.counts[key] = value.get<int>();
                        }
                    }

                    for (const auto& [key, value] : part_json.items()) {
                        if (key != "id" && key != "title" && key != "notes" &&
                            key != "repetitions" && key != "utterance" &&
                            key != "sequence" && key != "pairs" &&
                            key != "derived_counts") {
                            part.additional_data[key] = value;
                        }
                    }

                    parts.push_back(std::move(part));
                }
                section.parts = std::move(parts);
            }

            if (section_json.contains("derived_totals")) {
                for (const auto& [key, value] : section_json["derived_totals"].items()) {
                    section.counts[key] = value.get<int>();
                }
            }

            for (const auto& [key, value] : section_json.items()) {
                if (key != "id" && key != "title" && key != "steps" &&
                    key != "parts" && key != "iteration_marker" &&
                    key != "derived_totals" && key != "discipline_note") {
                    section.additional_data[key] = value;
                }
            }

            sections_.push_back(std::move(section));
        }
    }
}

std::optional<const Section*> RitualDefinition::findSection(const std::string& id) const {
    auto it = std::find_if(sections_.begin(), sections_.end(),
                          [&id](const Section& section) { return section.id == id; });
    return it != sections_.end() ? std::make_optional(&(*it)) : std::nullopt;
}

std::vector<std::string> RitualDefinition::getAllMarkers() const {
    std::vector<std::string> markers;

    for (const auto& section : sections_) {
        if (section.iteration_marker) {
            markers.push_back(section.iteration_marker->canonical);
            markers.insert(markers.end(),
                         section.iteration_marker->variants.begin(),
                         section.iteration_marker->variants.end());
        }

        if (section.steps) {
            for (const auto& step : *section.steps) {
                if (step.marker) {
                    markers.push_back(step.marker->canonical);
                    markers.insert(markers.end(),
                                 step.marker->variants.begin(),
                                 step.marker->variants.end());
                }
            }
        }
    }

    return markers;
}

std::optional<int> RitualDefinition::getCooldownForMarker(const std::string& marker) const {
    for (const auto& section : sections_) {
        if (section.iteration_marker) {
            if (section.iteration_marker->canonical == marker ||
                std::find(section.iteration_marker->variants.begin(),
                         section.iteration_marker->variants.end(),
                         marker) != section.iteration_marker->variants.end()) {
                return section.iteration_marker->cooldown_ms;
            }
        }

        if (section.steps) {
            for (const auto& step : *section.steps) {
                if (step.marker) {
                    if (step.marker->canonical == marker ||
                        std::find(step.marker->variants.begin(),
                                step.marker->variants.end(),
                                marker) != step.marker->variants.end()) {
                        return step.marker->cooldown_ms;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

void RitualDefinition::loadMantrasFromJson(const JsonValue& json) {
    std::cout << "Loading mantras from JSON..." << std::endl;
    std::cout << "JSON content type: " << json.type_name() << std::endl;

    mantras_.clear();

    const JsonValue* mantrasObject = nullptr;
    
    if (json.contains("mantras") && json["mantras"].is_object()) {
        mantrasObject = &json["mantras"];
    } else if (json.is_object()) {
        mantrasObject = &json;
    }

    if (!mantrasObject) {
        std::cerr << "Warning: No mantras object found in expected locations" << std::endl;
        return;
    }

    mantras_ = mantrasObject->get<MantraMap>();
    std::cout << "Successfully loaded " << mantras_.size() << " mantras" << std::endl;
}
}