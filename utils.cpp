/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Tiago Salem Herrmann
 * Copyright (c) 2014 N.Sukegawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "utils.h"

#include <QDebug>
#include <QScriptEngine>

void Utils::hangishMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!qEnvironmentVariableIsSet("HANGISH_DEBUG")) {
         return;
    }

    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

QVariantList Utils::jsArrayToVariantList(const QString &jsArray) {
    QScriptEngine engine;
    QScriptValue tree = engine.evaluate(jsArray);
    return tree.toVariant().toList();
}

void Utils::setMessage(const Reflection& ref,
                       Message& msg,
                       const FieldDescriptor* field,
                       const QVariant& value) {
    Message *sub_msg = ref.MutableMessage(&msg, field);
    Q_ASSERT(sub_msg);
    if (value.canConvert(QMetaType::QVariantList)) {
        packToMessage(value.value<QVariantList>(), *sub_msg);
    }
}

void Utils::setReflectionValue(const Reflection& ref,
                        Message& msg,
                        const FieldDescriptor* field,
                        const QVariant& value) {
    const EnumValueDescriptor * descriptor = NULL;
    switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
        ref.SetInt32(&msg, field, value.value<int>());
        break;
    case FieldDescriptor::CPPTYPE_INT64:
        ref.SetInt64(&msg, field, static_cast<int64>(value.value<qint64>()));
        break;
    case FieldDescriptor::CPPTYPE_UINT32:
        ref.SetUInt32(&msg, field, value.value<uint32_t>());
        break;
    case FieldDescriptor::CPPTYPE_UINT64:
        ref.SetUInt64(&msg, field, static_cast<uint64>(value.value<quint64>()));
        break;
    case FieldDescriptor::CPPTYPE_DOUBLE:
        ref.SetDouble(&msg, field, value.value<double>());
        break;
    case FieldDescriptor::CPPTYPE_FLOAT:
        ref.SetFloat(&msg, field, value.value<float>());
        break;
    case FieldDescriptor::CPPTYPE_BOOL:
        ref.SetBool(&msg, field, value.value<bool>());
        break;
    case FieldDescriptor::CPPTYPE_STRING:
        ref.SetString(&msg, field, value.value<QString>().toStdString());
        break;
    case FieldDescriptor::CPPTYPE_ENUM:
        descriptor = field->enum_type()->FindValueByNumber(value.value<int>());
        if (descriptor) {
            ref.SetEnum(&msg, field, descriptor);
        }
        break;
    case FieldDescriptor::CPPTYPE_MESSAGE:
        setMessage(ref, msg, field, value);
        break;
    }
}
// TODO: Handle invalid QVariant that cannot be converted
void Utils::setReflectionRepeatedValue(const Reflection& ref,
                                Message& msg,
                                const FieldDescriptor* field,
                                const QVariantList& list,
                                int size) {
#define PROTOBUF_QML_ADD_REPEATED(TYPE_ENUM, TYPE, CPP_TYPE)           \
    case FieldDescriptor::CPPTYPE_##TYPE_ENUM:                           \
    for (int i = 0; i < size; i++)                                     \
    ref.Add##TYPE(&msg, field, CPP_TYPE(list[i].value<CPP_TYPE>())); \
    break;

    switch (field->cpp_type()) {
    PROTOBUF_QML_ADD_REPEATED(INT32, Int32, int32);
    PROTOBUF_QML_ADD_REPEATED(INT64, Int64, int64);
    PROTOBUF_QML_ADD_REPEATED(UINT32, UInt32, uint32);
    PROTOBUF_QML_ADD_REPEATED(UINT64, UInt64, uint64);
    PROTOBUF_QML_ADD_REPEATED(DOUBLE, Double, double);
    PROTOBUF_QML_ADD_REPEATED(FLOAT, Float, float);
    PROTOBUF_QML_ADD_REPEATED(BOOL, Bool, bool);
    case FieldDescriptor::CPPTYPE_STRING:
        for (int i = 0; i < size; i++)
            ref.AddString(&msg, field, list[i].value<QString>().toStdString());
        break;
    case FieldDescriptor::CPPTYPE_ENUM:
        for (int i = 0; i < size; i++)
            ref.AddEnum(&msg, field, field->enum_type()->FindValueByNumber(
                            list[i].value<int>()));
        break;
    case FieldDescriptor::CPPTYPE_MESSAGE:
        for (int i = 0; i < size; i++) {
            packToMessage(list[i].value<QVariantList>(),
                          *ref.AddMessage(&msg, field));
        }
        break;
    }
#undef PROTOBUF_QML_SET_REPEATED
}

bool Utils::packToMessage(const QVariantList& fields,
                          Message& msg) {
    const Reflection *reflection = msg.GetReflection();
    const Descriptor *descriptor = msg.GetDescriptor();
    int field_count = descriptor->field_count();
    for (int i = 0; i < field_count && i < fields.size(); ++i) {
        QVariant field = fields[i];
        const FieldDescriptor *desc = descriptor->field(i);
        if (!desc->containing_oneof() && field.isValid()) {
            if (desc->is_repeated()) {
                if (!field.canConvert(QMetaType::QVariantList)) {
                    qWarning() << "Invalid type for repeated field: "
                               << QString::fromStdString(desc->name());
                } else {
                    QVariantList list = field.value<QVariantList>();
                    int size = list.size();
                    if (size > 0) {
                        setReflectionRepeatedValue(*reflection, msg, desc, list, size);
                    }
                }
            } else {
                setReflectionValue(*reflection, msg, desc, field);
            }
        }
    }
    return true;
}

#define REPEATED_NUMBER_TO_STRING(TYPE_PROTOBUF, TO_FUNCTION) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    for (int j = 0; j < size; ++j)                  { \
    items << QString::number(reflection->TO_FUNCTION(msg, field, j));      \
    } \
    break;

#define REPEATED_STRING_TO_STRING(TYPE_PROTOBUF, TO_FUNCTION) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    for (int j = 0; j < size; ++j)                  \
    items << QString("\"") + QString(reflection->TO_FUNCTION(msg, field, j).c_str()) + QString("\"");      \
    break;

#define REPEATED_TO_STRING_ENUM(TYPE_PROTOBUF) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    for (int j = 0; j < size; ++j)                  \
    items << QString::number((reflection->GetRepeatedEnum(msg, field, j)->number()));                \
    break;

#define NUMBER_TO_STRING(TYPE_PROTOBUF, TO_FUNCTION) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    items << QString::number(reflection->TO_FUNCTION(msg, field));                \
    break;
#define NUMBER_TO_STRING_ENUM(TYPE_PROTOBUF) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    items << QString::number((reflection->GetEnum(msg, field)->number()));                 \
    break;

#define STRING_TO_STRING(TYPE_PROTOBUF, TO_FUNCTION) \
    case FieldDescriptor::CPPTYPE_##TYPE_PROTOBUF:        \
    items << QString("\"") + QString(reflection->TO_FUNCTION(msg, field).c_str()) + QString("\"");      \
    break;

QString Utils::msgToJsArray(Message &msg) {
    QStringList items;
    const Reflection *reflection = msg.GetReflection();
    const Descriptor *descriptor = msg.GetDescriptor();
    int field_count = descriptor->field_count();

    for (int i = 0; i < field_count; ++i) {
        const FieldDescriptor *field = descriptor->field(i);
        if (field->is_repeated()) {
            int size = reflection->FieldSize(msg,field);
            if (size == 0) {
                items << "[]";
                continue;
            }
            switch(field->cpp_type()) {
            REPEATED_NUMBER_TO_STRING(BOOL, GetRepeatedBool);
            REPEATED_NUMBER_TO_STRING(DOUBLE, GetRepeatedDouble);
            REPEATED_TO_STRING_ENUM(ENUM);
            REPEATED_NUMBER_TO_STRING(FLOAT, GetRepeatedFloat);
            REPEATED_NUMBER_TO_STRING(INT32, GetRepeatedInt32);
            REPEATED_NUMBER_TO_STRING(INT64, GetRepeatedInt64);
            REPEATED_NUMBER_TO_STRING(UINT32, GetRepeatedUInt32);
            REPEATED_NUMBER_TO_STRING(UINT64, GetRepeatedUInt64);
            REPEATED_STRING_TO_STRING(STRING, GetRepeatedString);
            case FieldDescriptor::CPPTYPE_MESSAGE:
                for (int j = 0; j < size; ++j) {
                    Message *msg2 = reflection->MutableRepeatedMessage(&msg, field,j);
                    items << msgToJsArray(*msg2);
                }
                break;
            }
        } else {
            if (!reflection->HasField(msg, field)) {
                if (!field->cpp_type() == field->CPPTYPE_MESSAGE) {
                    items << "[]";
                } else {
                    items << "null";
                }
                continue;
            }
            switch(field->cpp_type()) {
            NUMBER_TO_STRING(BOOL, GetBool);
            NUMBER_TO_STRING(DOUBLE, GetDouble);
            NUMBER_TO_STRING_ENUM(ENUM);
            NUMBER_TO_STRING(FLOAT, GetFloat);
            NUMBER_TO_STRING(INT32, GetInt32);
            NUMBER_TO_STRING(INT64, GetInt64);
            NUMBER_TO_STRING(UINT32, GetUInt32);
            NUMBER_TO_STRING(UINT64, GetUInt64);
            STRING_TO_STRING(STRING, GetString);
            case FieldDescriptor::CPPTYPE_MESSAGE:
                Message *msg2 = reflection->MutableMessage(&msg, field);
                items << msgToJsArray(*msg2);
                break;
            }
        }
    }
    // remove trailing empty (optional) fields
    for (int i = items.size()-1; i >= 0; --i) {
        if (items.at(i) == "null") {
            items.removeAt(i);
        } else {
            break;
        }
    }
    return "[" + items.join(",") + "]";
}

