// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Minimal Markdown -> RML converter for release notes. Supports the subset a
// changelog needs: ATX headings (#, ##, ###), bullet lists (-, *, +),
// **bold**, *italic* / _italic_, `inline code`, [text](url) links (rendered as
// clickable <a href> anchors -- opened in the system browser by RELEASE_NOTES_RML),
// horizontal rules (--- / *** / ___), and blank-line-separated paragraphs. Everything else
// passes through as RML-escaped text. The output is an RML fragment styled by
// RELEASE_NOTES_RML's stylesheet.

#ifndef RUBIDIUM_RELEASE_NOTES_MARKDOWN_H
#define RUBIDIUM_RELEASE_NOTES_MARKDOWN_H

namespace RUBIDIUM
{
   std::string Markdown_ToRml (const std::string& sMarkdown);
}

#endif // RUBIDIUM_RELEASE_NOTES_MARKDOWN_H
