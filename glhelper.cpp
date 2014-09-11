#include <QOpenGLFunctions_3_2_Core>
#include "glhelper.h"
#include <QFile>
#include <QDebug>
#include <QString>

void _check_gl_error(const char *file, int line)
{
    GLenum err = glGetError();

    while (err != GL_NO_ERROR)
    {
        QString error("UNKNOWN");

        if (err == GL_INVALID_OPERATION)
            error = QStringLiteral("INVALID_OPERATION");
        else if (err == GL_INVALID_ENUM)
            error = QStringLiteral("INVALID_ENUM");
        else if (err == GL_INVALID_VALUE)
            error = QStringLiteral("INVALID_VALUE");
        else if (err == GL_OUT_OF_MEMORY)
            error = QStringLiteral("OUT_OF_MEMORY");
        else if (err == GL_INVALID_FRAMEBUFFER_OPERATION)
            error = QStringLiteral("INVALID_FRAMEBUFFER_OPERATION");

        qWarning() << "GL_" << error << " - " << file << ":" << line << endl;
        err = glGetError();
    }
}

static inline GLint getShaderInt(QOpenGLFunctions_3_2_Core *glFuncs, GLuint program, GLenum pname)
{
    GLint value;
    glFuncs->glGetShaderiv(program, pname, &value);
    return value;
}

static inline GLint getProgramInt(QOpenGLFunctions_3_2_Core *glFuncs, GLuint program, GLenum pname)
{
    GLint value;
    glFuncs->glGetProgramiv(program, pname, &value);
    return value;
}

GLuint compileGLShaderFile(QOpenGLFunctions_3_2_Core *glFuncs, const QString &path, GLenum shaderType)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Shader compile failed, couldn't open " << path;
        return 0;
    }

    QByteArray source = file.readAll();
    if (source.isNull())
    {
        qWarning() << "Shader compile failed, couldn't read " << path;
        return 0;
    }

    GLuint shader = glFuncs->glCreateShader(shaderType);

    const char *source_str = source.data();

    glFuncs->glShaderSource(shader, 1, &source_str, nullptr);
    glFuncs->glCompileShader(shader);

    if (!getShaderInt(glFuncs, shader, GL_COMPILE_STATUS)) {
        GLint logSize = getShaderInt(glFuncs, shader, GL_INFO_LOG_LENGTH);
        char* logMsg = new char[logSize];
        glFuncs->glGetShaderInfoLog(shader, logSize, nullptr, logMsg);

        qWarning() << "Shader compile failed, couldn't build " << path;
        qWarning() << logMsg;

        delete[] logMsg;
        glFuncs->glDeleteShader(shader);
        return 0;
    }

    QTextStream(stdout) << "Compiled " << path << endl;

    return shader;
}

GLuint buildGLProgram(QOpenGLFunctions_3_2_Core *glFuncs, GLuint vertShader, GLuint fragShader)
{
    if (!vertShader || !fragShader)
        return 0;

    GLuint program = glFuncs->glCreateProgram();
    glFuncs->glAttachShader(program, vertShader);
    glFuncs->glAttachShader(program, fragShader);
    glFuncs->glBindFragDataLocation(program, 0, "fragColor");
    glFuncs->glLinkProgram(program);

    if (!getProgramInt(glFuncs, program, GL_LINK_STATUS)) {
        GLint logSize = getProgramInt(glFuncs, program, GL_INFO_LOG_LENGTH);
        char* logMsg = new char[logSize];
        glFuncs->glGetProgramInfoLog(program, logSize, nullptr, logMsg);

        qWarning() << "Failed to link shader";
        qWarning() << logMsg;

        delete[] logMsg;
        glFuncs->glDeleteProgram(program);
        program = 0;
    }

    return program;
}

