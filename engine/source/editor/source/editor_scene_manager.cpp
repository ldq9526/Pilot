#include <mutex>
#include "editor/include/editor.h"
#include "editor/include/editor_scene_manager.h"
#include "runtime/function/render/include/render/glm_wrapper.h"

#include "runtime/function/scene/scene_manager.h"
#include "render/glm_wrapper.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <runtime/function/framework/component/transform/transform_component.cpp>

#include <runtime/core/base/macro.h>
#include "runtime/function/framework/level/level.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/input/input_system.h"
#include "runtime/function/render/include/render/render.h"
#include "runtime/function/scene/scene_manager.h"
#include "runtime/function/ui/ui_system.h"

namespace Pilot
{
    void EditorSceneManager::initialize()
    {
        auto current_scene = SceneManager::getInstance().getCurrentScene();
        m_camera = current_scene->m_camera;
    }

    void EditorSceneManager::tick(float delta_time)
    {
        //todo: editor scene tick
    }


    float intersectPlaneRay(glm::vec3 normal, float d, glm::vec3 origin, glm::vec3 dir)
    {
        float deno = glm::dot(normal, dir);
        if (fabs(deno) < 0.0001)
        {
            deno = 0.0001;
        }

        return -(glm::dot(normal, origin) + d) / deno;
    }

    size_t EditorSceneManager::updateCursorOnAxis(
        Vector2 cursor_uv,
        Vector2 game_engine_window_size)
    {

        float   camera_fov = m_camera->getFovYDeprecated();
        Vector3 camera_forward = m_camera->forward();

        Vector3 camera_up = m_camera->up();
        Vector3 camera_right = m_camera->right();
        Vector3 camera_position = m_camera->position();

        if (m_selected_gobject_id == k_invalid_gobject_id)
        {
            return m_selected_axis;
        }
        RenderMesh* selected_aixs = getAxisMeshByType(m_axis_mode);
        m_selected_axis = 3;
        if (m_is_show_axis == false)
        {
            return m_selected_axis;
        }
        else
        {
            glm::mat4 model_matrix = GLMUtil::fromMat4x4(selected_aixs->m_model_matrix);
            glm::vec3 model_scale;
            glm::quat model_rotation;
            glm::vec3 model_translation;
            glm::vec3 model_skew;
            glm::vec4 model_perspective;
            glm::decompose(model_matrix, model_scale, model_rotation, model_translation, model_skew, model_perspective);
            float     window_forward = game_engine_window_size.y / 2.0f / glm::tan(glm::radians(camera_fov) / 2.0f);
            glm::vec2 screen_center_uv = glm::vec2(cursor_uv.x, 1 - cursor_uv.y) - glm::vec2(0.5, 0.5);
            glm::vec3 world_ray_dir =
                GLMUtil::fromVec3(camera_forward) * window_forward +
                GLMUtil::fromVec3(camera_right) * (float)game_engine_window_size.x * screen_center_uv.x +
                GLMUtil::fromVec3(camera_up) * (float)game_engine_window_size.y * screen_center_uv.y;

            glm::vec4 local_ray_origin =
                glm::inverse(model_matrix) * glm::vec4(GLMUtil::fromVec3(camera_position), 1.0f);
            glm::vec3 local_ray_origin_xyz = glm::vec3(local_ray_origin.x, local_ray_origin.y, local_ray_origin.z);
            glm::vec3 local_ray_dir = glm::normalize(glm::inverse(model_rotation)) * world_ray_dir;

            glm::vec3 plane_normals[3] = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };

            float plane_view_depth[3] = { intersectPlaneRay(plane_normals[0], 0, local_ray_origin_xyz, local_ray_dir),
                                         intersectPlaneRay(plane_normals[1], 0, local_ray_origin_xyz, local_ray_dir),
                                         intersectPlaneRay(plane_normals[2], 0, local_ray_origin_xyz, local_ray_dir) };

            glm::vec3 intersect_pt[3] = {
                local_ray_origin_xyz + plane_view_depth[0] * local_ray_dir, // yoz
                local_ray_origin_xyz + plane_view_depth[1] * local_ray_dir, // xoz
                local_ray_origin_xyz + plane_view_depth[2] * local_ray_dir  // xoy
            };

            if ((int)m_axis_mode == 0 || (int)m_axis_mode == 2) // transition axis & scale axis
            {
                const float DIST_THRESHOLD = 0.6f;
                const float EDGE_OF_AXIS_MIN = 0.1f;
                const float EDGE_OF_AXIS_MAX = 2.0f;
                const float AXIS_LENGTH = 2.0f;

                float max_dist = 0.0f;
                // whether the ray (camera to mouse point) on any plane
                for (int i = 0; i < 3; ++i)
                {
                    float local_ray_dir_proj = glm::abs(glm::dot(local_ray_dir, plane_normals[i]));
                    float cos_alpha = local_ray_dir_proj / 1.0f; // local_ray_dir_proj / local_ray_dir.length
                    if (cos_alpha <= 0.15)                                // cos(80deg)~cps(100deg)
                    {
                        int   index00 = (i + 1) % 3;
                        int   index01 = 3 - i - index00;
                        int   index10 = (i + 2) % 3;
                        int   index11 = 3 - i - index10;
                        float axis_dist = (glm::abs(intersect_pt[index00][i]) + glm::abs(intersect_pt[index10][i])) / 2;
                        if (axis_dist > DIST_THRESHOLD) // too far from axis
                        {
                            continue;
                        }
                        // which axis is closer
                        if ((intersect_pt[index00][index01] > EDGE_OF_AXIS_MIN) &&
                            (intersect_pt[index00][index01] < AXIS_LENGTH) &&
                            (intersect_pt[index00][index01] > max_dist) &&
                            (glm::abs(intersect_pt[index00][i]) < EDGE_OF_AXIS_MAX))
                        {
                            max_dist = intersect_pt[index00][index01];
                            m_selected_axis = index01;
                        }
                        if ((intersect_pt[index10][index11] > EDGE_OF_AXIS_MIN) &&
                            (intersect_pt[index10][index11] < AXIS_LENGTH) &&
                            (intersect_pt[index10][index11] > max_dist) &&
                            (glm::abs(intersect_pt[index10][i]) < EDGE_OF_AXIS_MAX))
                        {
                            max_dist = intersect_pt[index10][index11];
                            m_selected_axis = index11;
                        }
                    }
                }
                // check axis
                if (m_selected_axis == 3)
                {
                    float min_dist = 1e10f;
                    for (int i = 0; i < 3; ++i)
                    {
                        int   index0 = (i + 1) % 3;
                        int   index1 = (i + 2) % 3;
                        float dist =
                            glm::pow(intersect_pt[index0][index1], 2) + glm::pow(intersect_pt[index1][index0], 2);
                        if ((intersect_pt[index0][i] > EDGE_OF_AXIS_MIN) &&
                            (intersect_pt[index0][i] < EDGE_OF_AXIS_MAX) && (dist < DIST_THRESHOLD) &&
                            (dist < min_dist))
                        {
                            min_dist = dist;
                            m_selected_axis = i;
                        }
                    }
                }
            }
            else if ((int)m_axis_mode == 1) // rotation axis
            {
                const float DIST_THRESHOLD = 0.2f;

                float min_dist = 1e10f;
                for (int i = 0; i < 3; ++i)
                {
                    const float dist =
                        std::fabs(1 - std::hypot(intersect_pt[i].x, intersect_pt[i].y, intersect_pt[i].z));
                    if ((dist < DIST_THRESHOLD) && (dist < min_dist))
                    {
                        min_dist = dist;
                        m_selected_axis = i;
                    }
                }
            }
            else
            {
                return m_selected_axis;
            }
        }
        m_editor->setSceneSelectedAxis(m_selected_axis);
        return m_selected_axis;
    }

    RenderMesh* EditorSceneManager::getAxisMeshByType(EditorAxisMode axis_mode)
    {
        RenderMesh* axis_mesh = nullptr;
        switch (axis_mode)
        {
        case EditorAxisMode::TranslateMode:
            axis_mesh = &m_translation_axis;
            break;
        case EditorAxisMode::RotateMode:
            axis_mesh = &m_rotation_axis;
            break;
        case EditorAxisMode::ScaleMode:
            axis_mesh = &m_scale_aixs;
            break;
        default:
            break;
        }
        return axis_mesh;
    }

    void EditorSceneManager::drawSelectedEntityAxis()
    {
        std::shared_ptr<GObject> selected_object = getSelectedGObject().lock();

        if (g_is_editor_mode && selected_object != nullptr)
        {
            const TransformComponent* transform_component = selected_object->tryGetComponentConst(TransformComponent);
            std::vector<RenderMesh>   axis_meshs;

            Vector3    scale;
            Quaternion rotation;
            Vector3    translation;
            transform_component->getMatrix().decomposition(translation, scale, rotation);
            Matrix4x4 translation_matrix = Matrix4x4::getTrans(translation);
            Matrix4x4 scale_matrix = Matrix4x4::buildScaleMatrix(1.0f, 1.0f, 1.0f);
            Matrix4x4 axis_model_matrix = translation_matrix * scale_matrix;
            RenderMesh* selected_aixs = getAxisMeshByType(m_axis_mode);
            if (m_axis_mode == EditorAxisMode::TranslateMode || m_axis_mode == EditorAxisMode::RotateMode)
            {
                selected_aixs->m_model_matrix = axis_model_matrix;
            }
            else if (m_axis_mode == EditorAxisMode::ScaleMode)
            {
                selected_aixs->m_model_matrix = axis_model_matrix * Matrix4x4(rotation);

            }
            axis_meshs.push_back(*selected_aixs);
            SceneManager::getInstance().setAxisMesh(axis_meshs);
        }
        else
        {
            std::vector<RenderMesh> axis_meshs;
            SceneManager::getInstance().setAxisMesh(axis_meshs);
        }
    }

    std::weak_ptr<GObject> EditorSceneManager::getSelectedGObject() const
    {
        std::weak_ptr<GObject> selected_object;
        if (m_selected_gobject_id != k_invalid_gobject_id)
        {
            std::shared_ptr<Level> level = WorldManager::getInstance().getCurrentActiveLevel().lock();
            if (level != nullptr)
            {
                selected_object = level->getGObjectByID(m_selected_gobject_id);
            }
        }
        return selected_object;
    }

    void EditorSceneManager::onGObjectSelected(size_t selected_gobject_id)
    {
        if (selected_gobject_id == m_selected_gobject_id)
            return;

        m_selected_gobject_id = selected_gobject_id;

        std::shared_ptr<GObject> selected_gobject = getSelectedGObject().lock();
        if (selected_gobject)
        {
            const TransformComponent* transform_component = selected_gobject->tryGetComponentConst(TransformComponent);
            m_selected_object_matrix = transform_component->getMatrix();
        }

        drawSelectedEntityAxis();

        if (m_selected_gobject_id != k_invalid_gobject_id)
        {
            LOG_INFO("select game object " + std::to_string(m_selected_gobject_id));
        }
        else
        {
            LOG_INFO("no game object selected");
        }
    }

    void EditorSceneManager::onDeleteSelectedGObject()
    {
        // delete selected entity
        std::shared_ptr<GObject> selected_object = getSelectedGObject().lock();
        if (selected_object != nullptr)
        {
            std::shared_ptr<Level> current_active_level = WorldManager::getInstance().getCurrentActiveLevel().lock();
            if (current_active_level == nullptr)
                return;

            current_active_level->deleteGObjectByID(m_selected_gobject_id);
        }
        onGObjectSelected(k_invalid_gobject_id);
    }

    void EditorSceneManager::moveEntity(float     new_mouse_pos_x,
        float     new_mouse_pos_y,
        float     last_mouse_pos_x,
        float     last_mouse_pos_y,
        Vector2   engine_window_pos,
        Vector2   engine_window_size,
        size_t    cursor_on_axis,
        Matrix4x4 model_matrix)
    {
        std::shared_ptr<GObject> selected_object = getSelectedGObject().lock();
        if (selected_object == nullptr)
            return;

        float angularVelocity =
            18.0f / Math::max(engine_window_size.x, engine_window_size.y); // 18 degrees while moving full screen
        Vector2 delta_mouse_move_uv = { (new_mouse_pos_x - last_mouse_pos_x), (new_mouse_pos_y - last_mouse_pos_y) };

        Vector3    model_scale;
        Quaternion model_rotation;
        Vector3    model_translation;
        model_matrix.decomposition(model_translation, model_scale, model_rotation);

        Matrix4x4 axis_model_matrix = Matrix4x4::IDENTITY;
        axis_model_matrix.setTrans(model_translation);

        Matrix4x4 view_matrix = m_camera->getLookAtMatrix();
        Matrix4x4 proj_matrix = m_camera->getPersProjMatrix();

        Vector4 model_world_position_4(model_translation, 1.f);

        Vector4 model_origin_clip_position = proj_matrix * view_matrix * model_world_position_4;
        model_origin_clip_position /= model_origin_clip_position.w;
        Vector2 model_origin_clip_uv =
            Vector2((model_origin_clip_position.x + 1) / 2.0f, (model_origin_clip_position.y + 1) / 2.0f);

        Vector4 axis_x_local_position_4(1, 0, 0, 1);
        if (m_axis_mode == EditorAxisMode::ScaleMode)
        {
            axis_x_local_position_4 = Matrix4x4(model_rotation) * axis_x_local_position_4;
        }
        Vector4 axis_x_world_position_4 = axis_model_matrix * axis_x_local_position_4;
        axis_x_world_position_4.w = 1.0f;
        Vector4 axis_x_clip_position = proj_matrix * view_matrix * axis_x_world_position_4;
        axis_x_clip_position /= axis_x_clip_position.w;
        Vector2 axis_x_clip_uv((axis_x_clip_position.x + 1) / 2.0f, (axis_x_clip_position.y + 1) / 2.0f);
        Vector2 axis_x_direction_uv = axis_x_clip_uv - model_origin_clip_uv;
        axis_x_direction_uv.normalise();

        Vector4 axis_y_local_position_4(0, 1, 0, 1);
        if (m_axis_mode == EditorAxisMode::ScaleMode)
        {
            axis_y_local_position_4 = Matrix4x4(model_rotation) * axis_y_local_position_4;
        }
        Vector4 axis_y_world_position_4 = axis_model_matrix * axis_y_local_position_4;
        axis_y_world_position_4.w = 1.0f;
        Vector4 axis_y_clip_position = proj_matrix * view_matrix * axis_y_world_position_4;
        axis_y_clip_position /= axis_y_clip_position.w;
        Vector2 axis_y_clip_uv((axis_y_clip_position.x + 1) / 2.0f, (axis_y_clip_position.y + 1) / 2.0f);
        Vector2 axis_y_direction_uv = axis_y_clip_uv - model_origin_clip_uv;
        axis_y_direction_uv.normalise();

        Vector4 axis_z_local_position_4(0, 0, 1, 1);
        if (m_axis_mode == EditorAxisMode::ScaleMode)
        {
            axis_z_local_position_4 = Matrix4x4(model_rotation) * axis_z_local_position_4;
        }
        Vector4 axis_z_world_position_4 = axis_model_matrix * axis_z_local_position_4;
        axis_z_world_position_4.w = 1.0f;
        Vector4 axis_z_clip_position = proj_matrix * view_matrix * axis_z_world_position_4;
        axis_z_clip_position /= axis_z_clip_position.w;
        Vector2 axis_z_clip_uv((axis_z_clip_position.x + 1) / 2.0f, (axis_z_clip_position.y + 1) / 2.0f);
        Vector2 axis_z_direction_uv = axis_z_clip_uv - model_origin_clip_uv;
        axis_z_direction_uv.normalise();

        TransformComponent* transform_component = selected_object->tryGetComponent(TransformComponent);

        Matrix4x4 new_model_matrix(Matrix4x4::IDENTITY);
        if (m_axis_mode == EditorAxisMode::TranslateMode) // translate
        {
            Vector3 move_vector = { 0, 0, 0 };
            if (cursor_on_axis == 0)
            {
                move_vector.x = delta_mouse_move_uv.dotProduct(axis_x_direction_uv) * angularVelocity;
            }
            else if (cursor_on_axis == 1)
            {
                move_vector.y = delta_mouse_move_uv.dotProduct(axis_y_direction_uv) * angularVelocity;
            }
            else if (cursor_on_axis == 2)
            {
                move_vector.z = delta_mouse_move_uv.dotProduct(axis_z_direction_uv) * angularVelocity;
            }
            else
            {
                return;
            }

            Matrix4x4 translate_mat;
            translate_mat.makeTransform(move_vector, Vector3::UNIT_SCALE, Quaternion::IDENTITY);
            new_model_matrix = axis_model_matrix * translate_mat;

            new_model_matrix = new_model_matrix * Matrix4x4(model_rotation);
            new_model_matrix =
                new_model_matrix * Matrix4x4::buildScaleMatrix(model_scale.x, model_scale.y, model_scale.z);

            Vector3    new_scale;
            Quaternion new_rotation;
            Vector3    new_translation;
            new_model_matrix.decomposition(new_translation, new_scale, new_rotation);

            Matrix4x4 translation_matrix = Matrix4x4::getTrans(new_translation);
            Matrix4x4 scale_matrix = Matrix4x4::buildScaleMatrix(1.f, 1.f, 1.f);
            Matrix4x4 axis_model_matrix = translation_matrix * scale_matrix;

            m_translation_axis.m_model_matrix = axis_model_matrix;
            m_rotation_axis.m_model_matrix = axis_model_matrix;
            m_scale_aixs.m_model_matrix = axis_model_matrix;
            std::vector<RenderMesh> axis_meshs;
            axis_meshs.push_back(m_translation_axis);
            SceneManager::getInstance().setAxisMesh(axis_meshs);

            transform_component->setPosition(new_translation);
            transform_component->setRotation(new_rotation);
            transform_component->setScale(new_scale);
        }
        else if (m_axis_mode == EditorAxisMode::RotateMode) // rotate
        {
            float   last_mouse_u = (last_mouse_pos_x - engine_window_pos.x) / engine_window_size.x;
            float   last_mouse_v = (last_mouse_pos_y - engine_window_pos.y) / engine_window_size.y;
            Vector2 last_move_vector(last_mouse_u - model_origin_clip_uv.x, last_mouse_v - model_origin_clip_uv.y);
            float   new_mouse_u = (new_mouse_pos_x - engine_window_pos.x) / engine_window_size.x;
            float   new_mouse_v = (new_mouse_pos_y - engine_window_pos.y) / engine_window_size.y;
            Vector2 new_move_vector(new_mouse_u - model_origin_clip_uv.x, new_mouse_v - model_origin_clip_uv.y);
            Vector3 delta_mouse_uv_3(delta_mouse_move_uv.x, delta_mouse_move_uv.y, 0);
            float   move_radian;
            Vector3 axis_of_rotation = { 0, 0, 0 };
            if (cursor_on_axis == 0)
            {
                move_radian = (delta_mouse_move_uv * angularVelocity).length();
                if (m_camera->forward().dotProduct(Vector3::UNIT_X) < 0)
                {
                    move_radian = -move_radian;
                }
                axis_of_rotation.x = 1;
            }
            else if (cursor_on_axis == 1)
            {
                move_radian = (delta_mouse_move_uv * angularVelocity).length();
                if (m_camera->forward().dotProduct(Vector3::UNIT_Y) < 0)
                {
                    move_radian = -move_radian;
                }
                axis_of_rotation.y = 1;
            }
            else if (cursor_on_axis == 2)
            {
                move_radian = (delta_mouse_move_uv * angularVelocity).length();
                if (m_camera->forward().dotProduct(Vector3::UNIT_Z) < 0)
                {
                    move_radian = -move_radian;
                }
                axis_of_rotation.z = 1;
            }
            else
            {
                return;
            }
            float move_direction = last_move_vector.x * new_move_vector.y - new_move_vector.x * last_move_vector.y;
            if (move_direction < 0)
            {
                move_radian = -move_radian;
            }

            Quaternion move_rot;
            move_rot.fromAngleAxis(Radian(move_radian), axis_of_rotation);
            new_model_matrix = axis_model_matrix * move_rot;
            new_model_matrix = new_model_matrix * Matrix4x4(model_rotation);
            new_model_matrix =
                new_model_matrix * Matrix4x4::buildScaleMatrix(model_scale.x, model_scale.y, model_scale.z);
            Vector3    new_scale;
            Quaternion new_rotation;
            Vector3    new_translation;

            new_model_matrix.decomposition(new_translation, new_scale, new_rotation);

            transform_component->setPosition(new_translation);
            transform_component->setRotation(new_rotation);
            transform_component->setScale(new_scale);
            m_scale_aixs.m_model_matrix = new_model_matrix;
        }
        else if (m_axis_mode == EditorAxisMode::ScaleMode) // scale
        {
            Vector3 delta_scale_vector = { 0, 0, 0 };
            Vector3 new_model_scale = { 0, 0, 0 };
            if (cursor_on_axis == 0)
            {
                delta_scale_vector.x = 0.01f;
                if (delta_mouse_move_uv.dotProduct(axis_x_direction_uv) < 0)
                {
                    delta_scale_vector = -delta_scale_vector;
                }
            }
            else if (cursor_on_axis == 1)
            {
                delta_scale_vector.y = 0.01f;
                if (delta_mouse_move_uv.dotProduct(axis_y_direction_uv) < 0)
                {
                    delta_scale_vector = -delta_scale_vector;
                }
            }
            else if (cursor_on_axis == 2)
            {
                delta_scale_vector.z = 0.01f;
                if (delta_mouse_move_uv.dotProduct(axis_z_direction_uv) < 0)
                {
                    delta_scale_vector = -delta_scale_vector;
                }
            }
            else
            {
                return;
            }
            new_model_scale = model_scale + delta_scale_vector;
            axis_model_matrix = axis_model_matrix * Matrix4x4(model_rotation);
            Matrix4x4 scale_mat;
            scale_mat.makeTransform(Vector3::ZERO, new_model_scale, Quaternion::IDENTITY);
            new_model_matrix = axis_model_matrix * scale_mat;
            Vector3    new_scale;
            Quaternion new_rotation;
            Vector3    new_translation;
            new_model_matrix.decomposition(new_translation, new_scale, new_rotation);

            transform_component->setPosition(new_translation);
            transform_component->setRotation(new_rotation);
            transform_component->setScale(new_scale);
        }
        setSelectedObjectMatrix(new_model_matrix);
    }
}
