const StdlibSurfaceMetadata *vectorHelperSurfaceMetadata();
const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadata();
const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadata();

bool resolvePublishedSemanticStdlibSurfaceMemberName(const SemanticProgram *semanticProgram,
                                                     const Expr &expr,
                                                     StdlibSurfaceId surfaceId,
                                                     std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceMemberName(std::string_view path,
                                             StdlibSurfaceId surfaceId,
                                             std::string &memberNameOut);
bool isPublishedStdlibSurfaceLoweringPath(std::string_view path,
                                          StdlibSurfaceId surfaceId);
bool isCanonicalPublishedStdlibSurfaceHelperPath(std::string_view path,
                                                 StdlibSurfaceId surfaceId);
