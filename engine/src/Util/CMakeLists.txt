target_sources(Engine PRIVATE
	"DirectoryChangeWatcher.cpp"
	"FilePicker.cpp"
	"ImguiStdlib.cpp"
	"ReadFile.cpp"
	"TmpDir.cpp"
)

# Move eastl to a separate library such include-what-you-use doesn't mess up the file.
add_library(EngineEastlAlloc STATIC "eastl.cpp")
target_link_libraries(EngineEastlAlloc PRIVATE ${EASTL_TARGET})
target_link_libraries(EngineEastlAlloc PRIVATE project_options)
target_link_libraries(Engine PRIVATE EngineEastlAlloc)
