#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

namespace sadhana {

// Forward declarations
struct Section;
struct Material;

// Define JSON type at the namespace level
using JsonValue = nlohmann::json;

// Generic action that can be performed during a ritual
struct RitualAction {
    std::string type;                    // Type of action (say, pour, gesture, etc.)
    std::string content;                 // What to say/do
    std::map<std::string, JsonValue> params;  // Additional parameters
};

// Generic marker for progress tracking
struct ProgressMarker {
    std::string canonical;               // Main form
    std::vector<std::string> variants;   // Alternative forms
    bool with_svaha_variants{false};     // Whether to auto-generate svaha variants
    int cooldown_ms{700};               // Minimum time between markers
    std::map<std::string, JsonValue> additional_params;  // For extension
};

// Generic part of a ritual section
struct Part {
    std::string id;
    std::string title;
    std::optional<int> repetitions;
    std::optional<std::string> utterance;
    std::optional<std::vector<std::string>> sequence;    // Generic sequence (beeja, steps, etc.)
    std::optional<std::vector<std::vector<std::string>>> pairs;  // For paired elements
    std::optional<std::map<std::string, RitualAction>> actions;
    std::map<std::string, int> counts;   // Generic count tracking
    std::string notes;
    JsonValue additional_data;           // For ritual-specific extensions
};

// Generic step in a ritual
struct Step {
    std::string id;
    std::string title;
    std::vector<std::string> items;
    std::vector<std::string> instructions;
    std::vector<std::string> mantra_refs;
    std::optional<ProgressMarker> marker;
    JsonValue additional_data;
};

// A section of the ritual
struct Section {
    std::string id;
    std::string title;
    std::optional<std::vector<Step>> steps;
    std::optional<ProgressMarker> iteration_marker;
    std::optional<std::vector<Part>> parts;
    std::map<std::string, int> counts;
    std::string notes;
    JsonValue additional_data;
};

// Material needed for the ritual
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

    // Core ritual information
    const std::string& getId() const { return id_; }
    const std::string& getTitle() const { return title_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getSource() const { return source_; }

    // Structured data access
    const MetadataMap& getMetadata() const { return metadata_; }
    const std::vector<Material>& getMaterials() const { return materials_; }
    const MantraMap& getMantras() const { return mantras_; }
    const std::vector<Section>& getSections() const { return sections_; }

    // Helper methods for ritual navigation
    std::optional<const Section*> findSection(const std::string& id) const;
    std::vector<std::string> getAllMarkers() const;
    std::optional<int> getCooldownForMarker(const std::string& marker) const;

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
    // Add this line with the other private function declarations
    void loadMantrasFromJson(const JsonValue& json);
};

} // namespace sadhana