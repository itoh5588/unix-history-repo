//===- FormatAdapters.h - Formatters for common LLVM types -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_FORMATADAPTERS_H
#define LLVM_SUPPORT_FORMATADAPTERS_H

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatCommon.h"
#include "llvm/Support/FormatVariadicDetails.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
template <typename T> class FormatAdapter : public detail::format_adapter {
protected:
  explicit FormatAdapter(T &&Item) : Item(Item) {}

  T Item;

  static_assert(!detail::uses_missing_provider<T>::value,
                "Item does not have a format provider!");
};

namespace detail {
template <typename T> class AlignAdapter final : public FormatAdapter<T> {
  AlignStyle Where;
  size_t Amount;

public:
  AlignAdapter(T &&Item, AlignStyle Where, size_t Amount)
      : FormatAdapter<T>(std::forward<T>(Item)), Where(Where), Amount(Amount) {}

  void format(llvm::raw_ostream &Stream, StringRef Style) {
    auto Adapter = detail::build_format_adapter(std::forward<T>(this->Item));
    FmtAlign(Adapter, Where, Amount).format(Stream, Style);
  }
};

template <typename T> class PadAdapter final : public FormatAdapter<T> {
  size_t Left;
  size_t Right;

public:
  PadAdapter(T &&Item, size_t Left, size_t Right)
      : FormatAdapter<T>(std::forward<T>(Item)), Left(Left), Right(Right) {}

  void format(llvm::raw_ostream &Stream, StringRef Style) {
    auto Adapter = detail::build_format_adapter(std::forward<T>(this->Item));
    Stream.indent(Left);
    Adapter.format(Stream, Style);
    Stream.indent(Right);
  }
};

template <typename T> class RepeatAdapter final : public FormatAdapter<T> {
  size_t Count;

public:
  RepeatAdapter(T &&Item, size_t Count)
      : FormatAdapter<T>(std::forward<T>(Item)), Count(Count) {}

  void format(llvm::raw_ostream &Stream, StringRef Style) {
    auto Adapter = detail::build_format_adapter(std::forward<T>(this->Item));
    for (size_t I = 0; I < Count; ++I) {
      Adapter.format(Stream, Style);
    }
  }
};
}

template <typename T>
detail::AlignAdapter<T> fmt_align(T &&Item, AlignStyle Where, size_t Amount) {
  return detail::AlignAdapter<T>(std::forward<T>(Item), Where, Amount);
}

template <typename T>
detail::PadAdapter<T> fmt_pad(T &&Item, size_t Left, size_t Right) {
  return detail::PadAdapter<T>(std::forward<T>(Item), Left, Right);
}

template <typename T>
detail::RepeatAdapter<T> fmt_repeat(T &&Item, size_t Count) {
  return detail::RepeatAdapter<T>(std::forward<T>(Item), Count);
}
}

#endif
