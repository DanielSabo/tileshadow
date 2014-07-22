#ifndef GLHELPER_H
#define GLHELPER_H

void _check_gl_error(const char *file, int line);
#define check_gl_error() _check_gl_error(__FILE__,__LINE__)

GLuint compileGLShaderFile(QOpenGLFunctions_3_2_Core *glFuncs, const QString &path, GLenum shaderType);
GLuint buildGLProgram(QOpenGLFunctions_3_2_Core *glFuncs, GLuint vertShader, GLuint fragShader);

#endif // GLHELPER_H
