#include "damageable_factory.h"
#include "core_vector.h"

#include <float.h>

/// Facet Configurations
/// One
const CoreVector minimum_one_configuration(-FLT_MAX, -FLT_MAX, -FLT_MAX);
const CoreVector maximum_one_configuration(FLT_MAX, FLT_MAX, FLT_MAX);

/// Two

/// Eight


DamageableObject DamageableFactory::CreateStandardObject(float shield,
                                                         float shield_regeneration,
                                                         FacetConfiguration shield_configuration,
                                                         float armor,
                                                         float hull) {
    Health hull_health(0, hull, hull, 0);
    Health armor_health(1, armor, armor, 0);
    Health shield_health(2, shield, shield, shield_regeneration);

    DamageableLayer hull_layer = CreateLayer(0, FacetConfiguration::one, hull_health, true);
    DamageableLayer armor_layer = CreateLayer(1, shield_configuration, armor_health, false);
    DamageableLayer shield_layer = CreateLayer(2, shield_configuration, shield_health, false);

    std::vector<DamageableLayer> layers = { hull_layer, armor_layer, shield_layer };

    DamageableObject object(layers, std::vector<DamageableObject>());
    return object;
}

DamageableLayer DamageableFactory::CreateLayer(int layer_index,
                                               FacetConfiguration configuration,
                                               Health health_template,
                                               bool core_layer) {
    health_template.layer = layer_index;
    int size = FacetConfigurationSize(configuration);

    std::vector<Health> facets;
    for(int i=0;i<size;i++) {
        facets.push_back(health_template);
    }

    DamageableLayer layer(layer_index, size, facets, core_layer);
    return layer;
}

DamageableLayer DamageableFactory::CreateLayer(int layer_index,
                                               FacetConfiguration configuration,
                                               float health_array[],
                                               float regeneration,
                                               bool core_layer) {
    int size = FacetConfigurationSize(configuration);

    std::vector<Health> facets;
    for(int i=0;i<size;i++) {
        Health health(layer_index, health_array[i], regeneration);
        facets.push_back(health);
    }

    DamageableLayer layer(layer_index, size, facets, core_layer);
    return layer;
}

