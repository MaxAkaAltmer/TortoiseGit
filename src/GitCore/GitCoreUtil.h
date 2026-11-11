#pragma once

// Internal helpers shared by gitcore implementation files.
// Not a part of the public gitcore API.

#include <git2.h>
#include <git2/sys/errors.h> // git_error_clear() lives in the "advanced" header

#include <string>
#include <utility>
#include <vector>

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

        // Keeps a list of strings (refspecs, pathsets) alive as a git_strarray.
        // Returns nullptr from get() for an empty list, which libgit2 reads as
        // "use the configured default".
        class Strings
        {
        public:
            explicit Strings(const std::vector<std::string>& values) : m_texts(values)
            {
                m_ptrs.resize(m_texts.size());
                for (std::size_t i = 0; i < m_texts.size(); ++i)
                    m_ptrs[i] = m_texts[i].data();
                m_array.strings = m_ptrs.empty() ? nullptr : m_ptrs.data();
                m_array.count   = static_cast<unsigned int>(m_ptrs.size());
            }

            Strings(const Strings&) = delete;
            Strings& operator=(const Strings&) = delete;

            const git_strarray* get() const { return m_array.count == 0 ? nullptr : &m_array; }
            bool empty() const { return m_array.count == 0; }

        private:
            std::vector<std::string> m_texts;
            std::vector<char*> m_ptrs;
            git_strarray m_array;
        };

        // Owns the strings of a git_strarray that a function fills into a
        // caller-allocated struct (git_remote_list and friends). The struct
        // lives here, on the stack; git_strarray_free releases its contents.
        class StrArrayContents
        {
        public:
            StrArrayContents() { m_array.strings = nullptr; m_array.count = 0; }
            ~StrArrayContents() { git_strarray_free(&m_array); }

            StrArrayContents(const StrArrayContents&) = delete;
            StrArrayContents& operator=(const StrArrayContents&) = delete;

            git_strarray* releaseSlot() { return &m_array; }
            std::size_t size() const { return m_array.count; }

            std::string at(std::size_t index) const
            {
                if (!m_array.strings || index >= m_array.count || !m_array.strings[index])
                    return {};
                return m_array.strings[index];
            }

        private:
            git_strarray m_array;
        };

        // libgit2's *_OPTIONS_INIT macros list only the leading members, which
        // -Wextra reports as -Wmissing-field-initializers although the result
        // is exactly what the library expects. Wrapping the initialization
        // keeps that warning out of the calling code.
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
        inline git_clone_options DefaultCloneOptions()
        {
            git_clone_options options = GIT_CLONE_OPTIONS_INIT;
            return options;
        }

        inline git_fetch_options DefaultFetchOptions()
        {
            git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
            return options;
        }

        inline git_push_options DefaultPushOptions()
        {
            git_push_options options = GIT_PUSH_OPTIONS_INIT;
            return options;
        }
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif

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
        using RemotePtr   = GitPtr<git_remote, git_remote_free>;
        using BufPtr      = GitPtr<git_buf, git_buf_dispose>;
    }
}
