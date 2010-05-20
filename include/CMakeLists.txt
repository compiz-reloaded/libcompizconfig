set (_headers
    ccs-backend.h
    ccs.h
    )

install (
	FILES ${_headers}
	DESTINATION ${COMPIZ_DESTDIR}${includedir}
	)
