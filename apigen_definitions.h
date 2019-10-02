#pragma once

#ifdef APIGEN_ACTIVE
#	define APIGEN_ANNOTATE(X) __attribute__((annotate(X)))
#else
#	define APIGEN_ANNOTATE(X)
#endif

#define APIGEN_STR_EXPAND(X) #X
#define APIGEN_STR(X) APIGEN_STR_EXPAND(X)


#define APIGEN_ANNOTATION_PREFIX                "apigen_"

#define APIGEN_ANNOTATION_EXPORT                APIGEN_ANNOTATION_PREFIX "export"
#define APIGEN_ANNOTATION_EXCLUDE               APIGEN_ANNOTATION_PREFIX "exclude"
#define APIGEN_ANNOTATION_RECURSIVE             APIGEN_ANNOTATION_PREFIX "recursive"

#define APIGEN_ANNOTATION_BASE                  APIGEN_ANNOTATION_PREFIX "base"
#define APIGEN_ANNOTATION_NOBASE                APIGEN_ANNOTATION_PREFIX "nobase"
#define APIGEN_ANNOTATION_BASETREE              APIGEN_ANNOTATION_PREFIX "basetree"

#define APIGEN_ANNOTATION_RENAME                APIGEN_ANNOTATION_PREFIX "rename"
#define APIGEN_ANNOTATION_RENAME_PREFIX         APIGEN_ANNOTATION_RENAME ":"
#define APIGEN_ANNOTATION_ADOPT_NAME            APIGEN_ANNOTATION_PREFIX "adopt_name"

#define APIGEN_ANNOTATION_CUSTOM_EXPORT         APIGEN_ANNOTATION_PREFIX "custom_export"
#define APIGEN_ANNOTATION_CUSTOM_EXPORT_PREFIX  APIGEN_ANNOTATION_CUSTOM_EXPORT ":"

#ifndef APIGEN_API_CLASS_NAME
#	define APIGEN_API_CLASS_NAME _apigen_api_impls
#endif
#define APIGEN_API_CLASS_NAME_STR APIGEN_STR(APIGEN_API_CLASS_NAME)
class APIGEN_API_CLASS_NAME;
#define APIGEN_ENABLE_PRIVATE_EXPORT friend ::APIGEN_API_CLASS_NAME


#define APIGEN_EXPORT                    APIGEN_ANNOTATE(APIGEN_ANNOTATION_EXPORT)
#define APIGEN_EXCLUDE                   APIGEN_ANNOTATE(APIGEN_ANNOTATION_EXCLUDE)
#define APIGEN_EXPORT_RECURSIVE          APIGEN_EXPORT APIGEN_ANNOTATE(APIGEN_ANNOTATION_RECURSIVE)

#define APIGEN_EXPORT_BASE               APIGEN_EXPORT APIGEN_ANNOTATE(APIGEN_ANNOTATION_BASE)
#define APIGEN_NOBASE                    APIGEN_ANNOTATE(APIGEN_ANNOTATION_NOBASE)
#define ALIGEN_EXPORT_BASETREE           APIGEN_EXPORT_BASE APIGEN_ANNOTATE(APIGEN_ANNOTATION_BASETREE)

#define APIGEN_RENAME(NAME)              APIGEN_ANNOTATE(APIGEN_ANNOTATION_RENAME_PREFIX APIGEN_STR(NAME))
#define APIGEN_ADOPT_NAME                APIGEN_ANNOTATE(APIGEN_ANNOTATION_ADOPT_NAME)

#define APIGEN_CUSTOM_EXPORT(ALT_TYPE, CONVERT_TO_ALT, CONVERT_FROM_ALT) \
	APIGEN_ANNOTATE(                                                     \
		APIGEN_ANNOTATION_CUSTOM_EXPORT_PREFIX                           \
		APIGEN_STR(ALT_TYPE) ","                                         \
		APIGEN_STR(CONVERT_TO_ALT) ","                                   \
		APIGEN_STR(CONVERT_FROM_ALT)                                     \
	)
