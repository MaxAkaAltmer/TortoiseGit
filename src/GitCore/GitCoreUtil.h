#pragma once

// Internal helpers shared by gitcore implementation files.
// Not a part of the public gitcore API.

#include <git2.h>
#include <git2/sys/errors.h> // git_error_clear() lives in the "advanced" header

#include <string>
#include <utility>

namespace GitCore
{
    namespace detail
    {
        // Clears the thread-local libgit2 error (needed after intentionally
        // ignored failures such as an iterator's terminating error code).
        inline void ClearError()
        {
            git_error_clear();
        }

        // One-time libgit2 initialization tied to the process lifetime.
        inline void EnsureInit()
        {
            struct LibGuard
            {
                LibGuard() { git_libgit2_init(); }
                ~LibGuard() { git_libgit2_shutdown(); }
                LibGuard(const LibGuard&) = delete;
                LibGuard& operator=(const LibGuard&) = delete;
            };
            static LibGuard guard;
        }

        // Owns a raw libgit2 pointer and releases it with the matching
        // git_*_free function.
        template <typename T, void (*Free)(T*)>
        class GitPtr
        {
        public:
            GitPtr() = default;
            explicit GitPtr(T* raw) : m_raw(raw) {}
            ~GitPtr() { reset(); }

            GitPtr(const GitPtr&) = delete;
            GitPtr& operator=(const GitPtr&) = delete;

            GitPtr(GitPtr&& other) noexcept : m_raw(other.m_raw) { other.m_raw = nullptr; }
            GitPtr& operator=(GitPtr&& other) noexcept
            {
                if (this != &other)
                {
                    reset(other.m_raw);
                    other.m_raw = nullptr;
                }
                return *this;
            }

            void reset(T* raw = nullptr)
            {
                if (m_raw)
                    Free(m_raw);
                m_raw = raw;
            }

            // Releases the pointer to the caller (out-parameter usage).
            T** releaseSlot()
            {
                reset();
                return &m_raw;
            }

            T* get() const { return m_raw; }
            T* operator->() const { return m_raw; }
            explicit operator bool() const { return m_raw != nullptr; }

        private:
            T* m_raw = nullptr;
        };

        inline std::string OidToHex(const git_oid* oid)
        {
            char buf[GIT_OID_MAX_HEXSIZE + 1] = {};
            git_oid_tostr(buf, sizeof(buf), oid);
            return buf;
        }

        // Keeps a one-entry pathspec alive for git_diff_options.
        // git_strarray::strings is char**, while std::string::data() yields a
        // non-const char* in C++17 - the library never modifies the string.
        class Pathspec
        {
        public:
            explicit Pathspec(const std::string& path) : m_text(path)
            {
                if (!m_text.empty())
                {
                    m_ptrs[0] = m_text.data();
                    m_array.strings = m_ptrs;
                    m_array.count = 1;
                }
                else
                {
                    m_array.strings = nullptr;
                    m_array.count = 0;
                }
            }

            Pathspec(const Pathspec&) = delete;
            Pathspec& operator=(const Pathspec&) = delete;

            const git_strarray& get() const { return m_array; }
            bool empty() const { return m_text.empty(); }

        private:
            std::string m_text;
            char* m_ptrs[1] = { nullptr };
            git_strarray m_array;
        };

        inline std::string TrimRight(std::string s)
        {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            return s;
        }

        using ConfigPtr = GitPtr<git_config, git_config_free>;
        using RevwalkPtr = GitPtr<git_revwalk, git_revwalk_free>;
        using CommitPtr = GitPtr<git_commit, git_commit_free>;
        using TreePtr = GitPtr<git_tree, git_tree_free>;
        using ObjectPtr = GitPtr<git_object, git_object_free>;
        using ReferencePtr = GitPtr<git_reference, git_reference_free>;
        using BranchEnumPtr = GitPtr<git_branch_iterator, git_branch_iterator_free>;
        using StatusListPtr = GitPtr<git_status_list, git_status_list_free>;
        using DiffPtr = GitPtr<git_diff, git_diff_free>;
        using StrArrayPtr = GitPtr<git_strarray, git_strarray_free>;
    }
}
