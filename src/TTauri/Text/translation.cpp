

#include "translation.hpp"
#include "po_parser.hpp"

namespace tt {

struct translation_key {
    std::string context;
    std::string msgid;
    language const *language;

    translation_key(std::string_view context, std::string_view msgid, tt::language const *language=nullptr) noexcept :
        context(context), msgid(msgid), language(language) {}

    [[nodiscard]] size_t hash() const noexcept {
        return hash_mix(language, context, msgid);
    }

    [[nodiscard]] friend bool operator==(translation_key const &lhs, translation_key const &rhs) noexcept {
        return lhs.language == rhs.language && lhs.context == rhs.context && lhs.msgid == rhs.msgid;
    }
};

}

namespace std {

template<>
struct hash<tt::translation_key> {
    [[nodiscard]] size_t operator()(tt::translation_key const &rhs) const noexcept {
        return rhs.hash();
    }
};

}

namespace tt {

std::unordered_map<translation_key,std::vector<std::string>> translations;

[[nodiscard]] std::string_view get_translation(
    std::string_view context,
    std::string_view msgid,
    long long n,
    std::vector<language*> const &languages
) noexcept {

    auto key = translation_key{context, msgid};

    for (ttlet *language : languages) {
        key.language = language;

        ttlet i = translations.find(key);
        if (i != translations.cend()) {
            ttlet plurality = language->plurality(n, ssize(i->second));
            return i->second[plurality];
        }
    }
    LOG_WARNING("No translation found for '{}'", msgid);
    return msgid;
}

void add_translation(
    std::string_view context,
    std::string_view msgid,
    language const &language,
    std::vector<std::string> const &plural_forms
) noexcept {
    auto key = translation_key{context, msgid, &language};
    translations[key] = plural_forms;
}

void add_translation(
    std::string_view context,
    std::string_view msgid,
    std::string const &language_tag,
    std::vector<std::string> const &plural_forms
) noexcept {
    ttlet &language = language::find_or_create(language_tag);
    add_translation(context, msgid, language, plural_forms);
}

void add_translation(po_translations const &po_translations, language const &language) noexcept
{
    for (ttlet &translation : po_translations.translations) {
        add_translation(
            translation.msgctxt,
            translation.msgid,
            language,
            translation.msgstr
        );
    }
}

}