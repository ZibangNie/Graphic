/*
 * Transform.h
 *
 * Purpose:
 *   Provides a minimal hierarchical transform component supporting:
 *     - Local TRS (translation, rotation, scale)
 *     - Parent/child relationships (scene graph linkage)
 *     - Lazy world-matrix evaluation with dirty propagation
 *
 * Conventions:
 *   - Local transform order: T * R * S.
 *   - Rotations are stored as quaternions (glm::quat, wxyz layout).
 *   - Euler helper composes rotations in X then Y then Z order (applied as qz * qy * qx).
 *
 * Performance model:
 *   - worldMatrix() caches the computed matrix.
 *   - Any local change or hierarchy change marks the transform dirty and recursively
 *     propagates to descendants to ensure correct world matrices.
 *
 * Ownership / lifetime:
 *   - Parent/child relationships are non-owning pointers.
 *   - The owner of Transform objects (e.g., SceneNode) must ensure stable addresses.
 */

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <algorithm>

class Transform {
public:
    Transform() = default;

    // ---- Local TRS setters ----
    // Notes:
    //   - Any setter marks the transform (and descendants) dirty.
    //   - Scale should remain non-zero on all axes to avoid singular matrices.
    void setLocalPosition(const glm::vec3& p) { m_localPos = p; markDirty(); }
    void setLocalRotation(const glm::quat& q) { m_localRot = q; markDirty(); }
    void setLocalScale(const glm::vec3& s)    { m_localScale = s; markDirty(); }

    /*
     * Sets local rotation from Euler angles in degrees.
     *
     * Parameters:
     *   eulerDeg : (pitchX, yawY, rollZ) in degrees.
     *
     * Notes:
     *   - Composition order is X then Y then Z, implemented as qz * qy * qx.
     *   - This is a convenience utility; quaternions should be preferred for animation.
     */
    void setLocalRotationEulerDeg(const glm::vec3& eulerDeg) {
        // order: X then Y then Z
        glm::quat qx = glm::angleAxis(glm::radians(eulerDeg.x), glm::vec3(1,0,0));
        glm::quat qy = glm::angleAxis(glm::radians(eulerDeg.y), glm::vec3(0,1,0));
        glm::quat qz = glm::angleAxis(glm::radians(eulerDeg.z), glm::vec3(0,0,1));
        setLocalRotation(qz * qy * qx);
    }

    // Local TRS accessors.
    const glm::vec3& localPosition() const { return m_localPos; }
    const glm::quat& localRotation() const { return m_localRot; }
    const glm::vec3& localScale()    const { return m_localScale; }

    // ---- Hierarchy ----
    /*
     * Reparents this transform under newParent.
     *
     * Parameters:
     *   newParent : Parent transform pointer (nullptr = root).
     *
     * Notes:
     *   - Removes this transform from the previous parent's child list if present.
     *   - Adds this transform to the new parent's child list if non-null.
     *   - Marks this transform and descendants dirty.
     *   - Does not preserve world transform; local TRS remains unchanged.
     */
    void setParent(Transform* newParent) {
        if (m_parent == newParent) return;

        // Remove from old parent children list
        if (m_parent) {
            auto& sibs = m_parent->m_children;
            sibs.erase(std::remove(sibs.begin(), sibs.end(), this), sibs.end());
        }

        m_parent = newParent;

        // Add to new parent children list
        if (m_parent) {
            m_parent->m_children.push_back(this);
        }

        markDirty();
    }

    Transform* parent() const { return m_parent; }
    const std::vector<Transform*>& children() const { return m_children; }

    // ---- World ----
    /*
     * Returns the cached world matrix (computed lazily).
     *
     * Notes:
     *   - If dirty, computes world as:
     *       world = parent.world * local
     *     else:
     *       world = local
     *   - Marked mutable to allow caching in const contexts.
     */
    const glm::mat4& worldMatrix() const {
        if (m_dirty) {
            glm::mat4 local = localMatrix();
            if (m_parent) {
                m_world = m_parent->worldMatrix() * local;
            } else {
                m_world = local;
            }
            m_dirty = false;
        }
        return m_world;
    }

    /*
     * Convenience: extracts translation from the world matrix.
     *
     * Returns:
     *   World-space position (x,y,z).
     *
     * Notes:
     *   - Assumes glm column-major layout; translation is stored in the 4th column.
     */
    glm::vec3 worldPosition() const {
        const glm::mat4& w = worldMatrix();
        return glm::vec3(w[3][0], w[3][1], w[3][2]);
    }

private:
    /*
     * Marks this transform dirty and propagates to all descendants.
     *
     * Notes:
     *   - Called whenever local TRS or parent linkage changes.
     *   - Propagation ensures children will recompute worldMatrix() on demand.
     */
    void markDirty() {
        if (!m_dirty) m_dirty = true;
        // propagate to descendants
        for (Transform* c : m_children) {
            if (c) c->markDirty();
        }
    }

    /*
     * Computes local matrix from local TRS.
     *
     * Returns:
     *   Local matrix = T * R * S.
     */
    glm::mat4 localMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, m_localPos);
        m *= glm::mat4_cast(m_localRot);
        m = glm::scale(m, m_localScale);
        return m;
    }

private:
    glm::vec3 m_localPos{0.f, 0.f, 0.f};
    glm::quat m_localRot{1.f, 0.f, 0.f, 0.f}; // (w,x,y,z)
    glm::vec3 m_localScale{1.f, 1.f, 1.f};

    Transform* m_parent = nullptr;
    std::vector<Transform*> m_children;

    // Cached world state (mutable to allow lazy evaluation in const methods).
    mutable bool m_dirty = true;
    mutable glm::mat4 m_world{1.f};
};
