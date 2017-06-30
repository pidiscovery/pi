file(GLOB HEADERS "include/graphene/incnetive_history/*.hpp")

add_library( graphene_incentive_history 
             incentive_history_plugin.cpp
           )

target_link_libraries( graphene_incentive_history graphene_chain graphene_app )
target_include_directories( graphene_incentive_history
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( incentive_history_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_incentive_history

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/incentive_history" )

