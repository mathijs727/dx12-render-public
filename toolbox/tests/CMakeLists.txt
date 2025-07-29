add_executable(ToolboxTest
	"src/bitwise_enum.cpp"
	"src/bit_ops.cpp"
	"src/error_handling.cpp"
	"src/hashmap_helper.cpp"
	"src/string.cpp"
	"src/template_meta.cpp"
	"src/variant_helper.cpp"
	"src/format.cpp")
target_link_libraries(ToolboxTest PRIVATE
	project_options
	project_warnings
	Catch2::Catch2WithMain
	Toolbox
)
CATCH_DISCOVER_TESTS(ToolboxTest)