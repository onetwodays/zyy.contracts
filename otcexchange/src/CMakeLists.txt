project(otcexchange)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)


add_contract( otcexchange otcexchange otcexchange.cpp   )
target_include_directories( otcexchange PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_include_directories( otcexchange PUBLIC ${CMAKE_SOURCE_DIR}/../..)# 为了找到common
target_include_directories( otcexchange PUBLIC ${CMAKE_SOURCE_DIR}/../../common)# 为了找到common
target_include_directories( otcexchange PUBLIC /usr/local/include)# 为了找到common
target_ricardian_directory( otcexchange ${CMAKE_SOURCE_DIR}/../ricardian )