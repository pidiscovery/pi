file(GLOB HEADERS "include/graphene/transaction_record/*.hpp")

add_library( graphene_transaction_record
             transaction_record_plugin.cpp
           )

target_link_libraries( graphene_transaction_record graphene_chain graphene_app )
target_include_directories( graphene_transaction_record
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( transaction_record_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_transaction_record

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/transaction_record" )

