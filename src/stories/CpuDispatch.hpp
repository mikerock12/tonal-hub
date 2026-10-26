#pragma once
namespace tonal::stories {
constexpr bool acceleratedReaderSupported(bool arm64,bool neon,bool scalarFp16,bool vectorFp16,bool dotprod,bool i8mm,bool atomics){
 return arm64&&neon&&scalarFp16&&vectorFp16&&dotprod&&i8mm&&atomics;
}
}
