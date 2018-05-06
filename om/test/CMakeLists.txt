function(omr_add_om_test testname)
	add_executable("${testname}"
		"${testname}.cpp"
		main.cpp
	)
	target_link_libraries("${testname}"
		PUBLIC
			omrgc
			omrGtestGlue
	)
	add_test(
		NAME    "${testname}"
		COMMAND "${testname}"
	)
	set_property(
		TARGET "${testname}"
		PROPERTY CXX_STANDARD 11
	)
endfunction(omr_add_om_test)

omr_add_om_test(MemoryManagerTest)
omr_add_om_test(RuntimeTest)
omr_add_om_test(ValueTest)
omr_add_om_test(RootTest)
omr_add_om_test(DoubleTest)
