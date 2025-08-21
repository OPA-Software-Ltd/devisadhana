#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>

namespace sadhana {

struct Section;
struct Material;

using JsonValue = nlohmann::json;

struct RitualAction {
    std::string type;
    std::string content;
    std::map<std::string, JsonValue> params;
};

struct ProgressMarker {
    std::string canonical;
    std::vector<std::string> variants;
    bool with_svaha_variants{false};
    int cooldown_ms{700};
    std::map<std::string, JsonValue> additional_params;
};

struct Part {
    std::string id;
    std::string title;
    std::optional<std::string> description;    
    std::optional<int> repetitions;
    std::optional<std::string> utterance;
    std::optional<std::string> mantra_ref;    // Add this line
    std::optional<std::vector<std::string>> sequence;
    std::optional<std::vector<std::vector<std::string>>> pairs;
    std::optional<std::map<std::string, RitualAction>> actions;
    std::map<std::string, int> counts;
    std::string notes;
    JsonValue additional_data;
};

struct Step {
    std::string id;
    std::string title;
    std::vector<std::string> items;
    std::vector<std::string> instructions;
    std::vector<std::string> mantra_refs;
    std::optional<ProgressMarker> marker;
    JsonValue additional_data;
};

struct Section {
    std::string id;
    std::string title;
    std::optional<std::string> description;    // Add this
    std::optional<std::string> introduction;   // Add this
    std::optional<std::vector<Step>> steps;
    std::optional<ProgressMarker> iteration_marker;
    std::optional<std::vector<Part>> parts;
    std::map<std::string, int> counts;
    std::string notes;
    JsonValue additional_data;
};

struct Material {
    std::string id;
    std::string name;
    std::string details;
    bool optional{false};
    JsonValue additional_data;
};

class RitualDefinition {
public:
    using MetadataMap = std::map<std::string, JsonValue>;
    using MantraMap = std::map<std::string, JsonValue>;

    bool loadFromFile(const std::string& filepath);
    bool loadFromJson(const JsonValue& json);

    const std::string& getId() const { return id_; }
    const std::string& getTitle() const { return title_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getSource() const { return source_; }

    const MetadataMap& getMetadata() const { return metadata_; }
    const std::vector<Material>& getMaterials() const { return materials_; }
    const MantraMap& getMantras() const { return mantras_; }
    const std::vector<Section>& getSections() const { return sections_; }

    std::optional<const Section*> findSection(const std::string& id) const;
    std::vector<std::string> getAllMarkers() const;
    std::optional<int> getCooldownForMarker(const std::string& marker) const;
    
    std::string getCurrentMantra(const std::string& sectionId, const std::string& partId) const;
    int getRequiredRepetitions(const std::string& partId) const;

    struct CurrentState {
        std::string expectedUtterance;
        std::string description;
        int requiredRepetitions{1};
        bool isComplete{false};
    };

    CurrentState getCurrentState(const std::string& sectionId, const std::string& partId) const;

private:
    std::string id_;
    std::string title_;
    std::string version_;
    std::string source_;
    MetadataMap metadata_;
    std::vector<Material> materials_;
    MantraMap mantras_;
    std::vector<Section> sections_;

    void parseFromJson(const JsonValue& json);
    void loadMaterialsFromJson(const JsonValue& json);
    void loadProceduresFromJson(const JsonValue& json);
    void loadMantrasFromJson(const JsonValue& json);
};

} // namespace sadhana