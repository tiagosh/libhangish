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

#ifndef UTILS_H
#define UTILS_H

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>
#include <QString>
#include <QVariantList>
#include "types.h"

using namespace google::protobuf;
using google::protobuf::Message;


class Utils
{

public:
    static QVariantList jsArrayToVariantList(const QString &jsArray);
    static bool packToMessage(const QVariantList& fields, Message& msg);
    static QString msgToJsArray(Message &msg);
    static void hangishMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    static void setMessage(const Reflection& ref,
                           Message& msg,
                           const FieldDescriptor* field,
                           const QVariant& value);

    static void setReflectionValue(const Reflection& ref,
                            Message& msg,
                            const FieldDescriptor* field,
                            const QVariant& value);

    static void setReflectionRepeatedValue(const Reflection& ref,
                                    Message& msg,
                                    const FieldDescriptor* field,
                                    const QVariantList& list,
                                    int size);

};

#endif // UTILS_H
