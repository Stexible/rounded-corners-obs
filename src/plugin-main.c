#include <obs-module.h>
#include <graphics/graphics.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("rounded-corners", "en-US")

#define FILTER_ID "rounded_corners_filter"

static const char *radius_keys[] = {
    "radius_tl", "radius_tr", "radius_br", "radius_bl"
};

static const char *shape_keys[] = {
    "shape_tl", "shape_tr", "shape_br", "shape_bl"
};

struct rounded_corners_data {
    obs_source_t *context;
    gs_effect_t *effect;
    gs_eparam_t *image_size_param;
    gs_eparam_t *radii_param;
    gs_eparam_t *shape_param;
    float radii[4];
    float shape[4];
    bool sync_radii;
};

static const char *rounded_corners_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("RoundedCorners.Name");
}

static void rounded_corners_update(void *data, obs_data_t *settings)
{
    struct rounded_corners_data *filter = data;

    filter->sync_radii = obs_data_get_bool(settings, "sync_radii");

    for (size_t i = 0; i < 4; i++) {
        filter->radii[i] =
            (float)obs_data_get_double(settings, radius_keys[i]);
        filter->shape[i] =
            (float)obs_data_get_double(settings, shape_keys[i]);
    }
}

static void *rounded_corners_create(obs_data_t *settings, obs_source_t *source)
{
    struct rounded_corners_data *filter = bzalloc(sizeof(*filter));
    filter->context = source;

    char *path = obs_module_file("effects/rounded-corners.effect");

    if (path) {
        obs_enter_graphics();
        filter->effect = gs_effect_create_from_file(path, NULL);
        obs_leave_graphics();
        bfree(path);
    }

    if (!filter->effect) {
        blog(LOG_ERROR, "[rounded-corners] Failed to load effect");
        bfree(filter);
        return NULL;
    }

    filter->image_size_param =
        gs_effect_get_param_by_name(filter->effect, "image_size");
    filter->radii_param =
        gs_effect_get_param_by_name(filter->effect, "corner_radii");
    filter->shape_param =
        gs_effect_get_param_by_name(filter->effect, "corner_shape");

    rounded_corners_update(filter, settings);
    return filter;
}

static void rounded_corners_destroy(void *data)
{
    struct rounded_corners_data *filter = data;

    if (!filter)
        return;

    if (filter->effect) {
        obs_enter_graphics();
        gs_effect_destroy(filter->effect);
        obs_leave_graphics();
    }

    bfree(filter);
}

static void rounded_corners_defaults(obs_data_t *settings)
{
    for (size_t i = 0; i < 4; i++) {
        obs_data_set_default_double(settings, radius_keys[i], 10.0);
        obs_data_set_default_double(settings, shape_keys[i], 0.0);
    }

    obs_data_set_default_double(settings, "radius", 10.0);
    obs_data_set_default_double(settings, "shape", 0.0);
    obs_data_set_default_bool(settings, "sync_radii", false);
}

static void set_visibility(obs_properties_t *props, bool sync)
{
    obs_property_set_visible(obs_properties_get(props, "radius"), sync);
    obs_property_set_visible(obs_properties_get(props, "shape"), sync);

    for (size_t i = 0; i < 4; i++) {
        obs_property_set_visible(
            obs_properties_get(props, radius_keys[i]), !sync);
        obs_property_set_visible(
            obs_properties_get(props, shape_keys[i]), !sync);
    }
}

static bool sync_toggle(void *priv, obs_properties_t *props,
                        obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(priv);
    UNUSED_PARAMETER(property);

    bool sync = obs_data_get_bool(settings, "sync_radii");

    if (sync) {
        double min_radius = obs_data_get_double(settings, radius_keys[0]);

        for (size_t i = 1; i < 4; i++) {
            double value = obs_data_get_double(settings, radius_keys[i]);
            if (value < min_radius)
                min_radius = value;
        }

        double shape = obs_data_get_double(settings, shape_keys[0]);

        obs_data_set_double(settings, "radius", min_radius);
        obs_data_set_double(settings, "shape", shape);

        for (size_t i = 0; i < 4; i++) {
            obs_data_set_double(settings, radius_keys[i], min_radius);
            obs_data_set_double(settings, shape_keys[i], shape);
        }
    } else {
        double radius = obs_data_get_double(settings, "radius");
        double shape = obs_data_get_double(settings, "shape");

        for (size_t i = 0; i < 4; i++) {
            obs_data_set_double(settings, radius_keys[i], radius);
            obs_data_set_double(settings, shape_keys[i], shape);
        }
    }

    set_visibility(props, sync);
    return true;
}

static bool sync_radius_changed(void *priv, obs_properties_t *props,
                                obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(priv);
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);

    if (!obs_data_get_bool(settings, "sync_radii"))
        return false;

    double radius = obs_data_get_double(settings, "radius");

    for (size_t i = 0; i < 4; i++)
        obs_data_set_double(settings, radius_keys[i], radius);

    return false;
}

static bool sync_shape_changed(void *priv, obs_properties_t *props,
                               obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(priv);
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);

    if (!obs_data_get_bool(settings, "sync_radii"))
        return false;

    double shape = obs_data_get_double(settings, "shape");

    for (size_t i = 0; i < 4; i++)
        obs_data_set_double(settings, shape_keys[i], shape);

    return false;
}

static bool corner_radius_changed(void *priv, obs_properties_t *props,
                                  obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);

    if (obs_data_get_bool(settings, "sync_radii"))
        return false;

    size_t i = (size_t)(intptr_t)priv;
    obs_data_set_double(
        settings, "radius", obs_data_get_double(settings, radius_keys[i]));

    return false;
}

static bool corner_shape_changed(void *priv, obs_properties_t *props,
                                 obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);

    if (obs_data_get_bool(settings, "sync_radii"))
        return false;

    size_t i = (size_t)(intptr_t)priv;
    obs_data_set_double(
        settings, "shape", obs_data_get_double(settings, shape_keys[i]));

    return false;
}

static obs_properties_t *rounded_corners_properties(void *data)
{
    struct rounded_corners_data *filter = data;
    obs_properties_t *props = obs_properties_create();

    obs_property_t *sync = obs_properties_add_bool(
        props, "sync_radii", obs_module_text("RoundedCorners.SyncRadii"));
    obs_property_set_modified_callback2(sync, sync_toggle, NULL);

    obs_property_t *radius = obs_properties_add_float_slider(
        props, "radius", obs_module_text("RoundedCorners.Radius"),
        0.0, 100.0, 0.1);
    obs_property_float_set_suffix(radius, "%");
    obs_property_set_modified_callback2(radius, sync_radius_changed, NULL);

    obs_property_t *shape = obs_properties_add_float_slider(
        props, "shape", obs_module_text("RoundedCorners.Shape"),
        -1.0, 1.0, 0.01);
    obs_property_set_modified_callback2(shape, sync_shape_changed, NULL);

    const char *radius_labels[] = {
        "RoundedCorners.TopLeft",
        "RoundedCorners.TopRight",
        "RoundedCorners.BottomRight",
        "RoundedCorners.BottomLeft"
    };

    const char *shape_labels[] = {
        "RoundedCorners.ShapeTopLeft",
        "RoundedCorners.ShapeTopRight",
        "RoundedCorners.ShapeBottomRight",
        "RoundedCorners.ShapeBottomLeft"
    };

    for (intptr_t i = 0; i < 4; i++) {
        obs_property_t *r = obs_properties_add_float_slider(
            props, radius_keys[i], obs_module_text(radius_labels[i]),
            0.0, 100.0, 0.1);
        obs_property_float_set_suffix(r, "%");
        obs_property_set_modified_callback2(
            r, corner_radius_changed, (void *)i);

        obs_property_t *s = obs_properties_add_float_slider(
            props, shape_keys[i], obs_module_text(shape_labels[i]),
            -1.0, 1.0, 0.01);
        obs_property_set_modified_callback2(
            s, corner_shape_changed, (void *)i);
    }

    set_visibility(props, filter ? filter->sync_radii : false);
    return props;
}

/*
 * Radius is stored in percent: 100% = half of the shorter side of the
 * source. Each radius is therefore at most min(w, h) / 2, and two
 * neighbouring corners can never overlap.
 */
static void radii_to_pixels(const float pct[4], float w, float h, float out[4])
{
    float max_r = (w < h ? w : h) * 0.5f;

    for (int i = 0; i < 4; i++) {
        float p = pct[i];
        if (p < 0.0f)
            p = 0.0f;
        if (p > 100.0f)
            p = 100.0f;
        out[i] = p * 0.01f * max_r;
    }
}

static void rounded_corners_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    struct rounded_corners_data *filter = data;

    /* A filter has to take the size of the source it is attached to. */
    obs_source_t *target = obs_filter_get_target(filter->context);
    uint32_t width = target ? obs_source_get_base_width(target) : 0;
    uint32_t height = target ? obs_source_get_base_height(target) : 0;

    if (!width || !height) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (!obs_source_process_filter_begin(
            filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
        return;

    float fitted[4];
    radii_to_pixels(filter->radii, (float)width, (float)height, fitted);

    struct vec2 size = {(float)width, (float)height};
    struct vec4 radii = {fitted[0], fitted[1], fitted[2], fitted[3]};
    struct vec4 shape = {
        filter->shape[0], filter->shape[1],
        filter->shape[2], filter->shape[3]
    };

    gs_effect_set_vec2(filter->image_size_param, &size);
    gs_effect_set_vec4(filter->radii_param, &radii);
    gs_effect_set_vec4(filter->shape_param, &shape);

    obs_source_process_filter_end(
        filter->context, filter->effect, width, height);
}

static struct obs_source_info rounded_corners_filter = {
    .id = FILTER_ID,
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = rounded_corners_name,
    .create = rounded_corners_create,
    .destroy = rounded_corners_destroy,
    .update = rounded_corners_update,
    .get_defaults = rounded_corners_defaults,
    .get_properties = rounded_corners_properties,
    .video_render = rounded_corners_video_render,
};

bool obs_module_load(void)
{
    obs_register_source(&rounded_corners_filter);
    blog(LOG_INFO, "Rounded Corners filter loaded");
    return true;
}

const char *obs_module_description(void)
{
    return "GPU rounded-corner alpha mask filter for OBS Studio";
}