//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexArrayGL.cpp: Implements the class methods for VertexArrayGL.

#include "libANGLE/renderer/gl/VertexArrayGL.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

using namespace gl;

namespace rx
{
namespace
{
bool SameVertexAttribFormat(const VertexAttribute &a, const VertexAttribute &b)
{
    return a.format == b.format && a.relativeOffset == b.relativeOffset;
}

bool SameVertexBuffer(const VertexBinding &a, const VertexBinding &b)
{
    return a.getStride() == b.getStride() && a.getOffset() == b.getOffset() &&
           a.getBuffer().get() == b.getBuffer().get();
}

bool IsVertexAttribPointerSupported(size_t attribIndex, const VertexAttribute &attrib)
{
    return (attribIndex == attrib.bindingIndex && attrib.relativeOffset == 0);
}

GLuint GetAdjustedDivisor(GLuint numViews, GLuint divisor)
{
    return numViews * divisor;
}

static void ValidateStateHelperGetIntegerv(const FunctionsGL *functions,
                                           const GLuint localValue,
                                           const GLenum pname,
                                           const char *localName,
                                           const char *driverName)
{
    GLint queryValue;
    functions->getIntegerv(pname, &queryValue);
    if (localValue != static_cast<GLuint>(queryValue))
    {
        WARN() << localName << " (" << localValue << ") != " << driverName << " (" << queryValue
               << ")";
        // Re-add ASSERT: http://anglebug.com/3900
        // ASSERT(false);
    }
}

static void ValidateStateHelperGetVertexAttribiv(const FunctionsGL *functions,
                                                 const GLint index,
                                                 const GLuint localValue,
                                                 const GLenum pname,
                                                 const char *localName,
                                                 const char *driverName)
{
    GLint queryValue;
    functions->getVertexAttribiv(index, pname, &queryValue);
    if (localValue != static_cast<GLuint>(queryValue))
    {
        WARN() << localName << "[" << index << "] (" << localValue << ") != " << driverName << "["
               << index << "] (" << queryValue << ")";
        // Re-add ASSERT: http://anglebug.com/3900
        // ASSERT(false);
    }
}

}  // anonymous namespace

VertexArrayGL::VertexArrayGL(const VertexArrayState &state, GLuint id)
    : VertexArrayImpl(state),
      mVertexArrayID(id),
      mAppliedNumViews(1),
      mAppliedElementArrayBuffer(),
      mAppliedBindings(state.getMaxBindings()),
      mStreamingElementArrayBufferSize(0),
      mStreamingElementArrayBuffer(0),
      mStreamingArrayBufferSize(0),
      mStreamingArrayBuffer(0)
{
    // Set the cached vertex attribute array and vertex attribute binding array size
    GLuint maxVertexAttribs = static_cast<GLuint>(state.getMaxAttribs());
    for (GLuint i = 0; i < maxVertexAttribs; i++)
    {
        mAppliedAttributes.emplace_back(i);
    }
    mForcedStreamingAttributesFirstOffsets.fill(0);
}

VertexArrayGL::~VertexArrayGL() {}

void VertexArrayGL::destroy(const gl::Context *context)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->deleteVertexArray(mVertexArrayID);
    mVertexArrayID   = 0;
    mAppliedNumViews = 1;

    stateManager->deleteBuffer(mStreamingElementArrayBuffer);
    mStreamingElementArrayBufferSize = 0;
    mStreamingElementArrayBuffer     = 0;

    stateManager->deleteBuffer(mStreamingArrayBuffer);
    mStreamingArrayBufferSize = 0;
    mStreamingArrayBuffer     = 0;

    mAppliedElementArrayBuffer.set(context, nullptr);
    for (auto &binding : mAppliedBindings)
    {
        binding.setBuffer(context, nullptr);
    }
}

angle::Result VertexArrayGL::syncClientSideData(const gl::Context *context,
                                                const gl::AttributesMask &activeAttributesMask,
                                                GLint first,
                                                GLsizei count,
                                                GLsizei instanceCount) const
{
    return syncDrawState(context, activeAttributesMask, first, count,
                         gl::DrawElementsType::InvalidEnum, nullptr, instanceCount, false, nullptr);
}

void VertexArrayGL::updateElementArrayBufferBinding(const gl::Context *context) const
{
    gl::Buffer *elementArrayBuffer = mState.getElementArrayBuffer();
    if (elementArrayBuffer != nullptr && elementArrayBuffer != mAppliedElementArrayBuffer.get())
    {
        StateManagerGL *stateManager = GetStateManagerGL(context);
        const BufferGL *bufferGL     = GetImplAs<BufferGL>(elementArrayBuffer);
        stateManager->bindBuffer(gl::BufferBinding::ElementArray, bufferGL->getBufferID());
        mAppliedElementArrayBuffer.set(context, elementArrayBuffer);
    }
}

angle::Result VertexArrayGL::syncDrawState(const gl::Context *context,
                                           const gl::AttributesMask &activeAttributesMask,
                                           GLint first,
                                           GLsizei count,
                                           gl::DrawElementsType type,
                                           const void *indices,
                                           GLsizei instanceCount,
                                           bool primitiveRestartEnabled,
                                           const void **outIndices) const
{
    // Check if any attributes need to be streamed, determines if the index range needs to be
    // computed
    const gl::AttributesMask &needsStreamingAttribs =
        context->getStateCache().getActiveClientAttribsMask();

    // Determine if an index buffer needs to be streamed and the range of vertices that need to be
    // copied
    IndexRange indexRange;
    const angle::FeaturesGL &features = GetFeaturesGL(context);
    if (type != gl::DrawElementsType::InvalidEnum)
    {
        ANGLE_TRY(syncIndexData(context, count, type, indices, primitiveRestartEnabled,
                                needsStreamingAttribs.any(), &indexRange, outIndices));
    }
    else
    {
        // Not an indexed call, set the range to [first, first + count - 1]
        indexRange.start = first;
        indexRange.end   = first + count - 1;

        if (features.shiftInstancedArrayDataWithExtraOffset.enabled && first > 0)
        {
            gl::AttributesMask updatedStreamingAttribsMask = needsStreamingAttribs;
            auto candidateAttributesMask =
                mInstancedAttributesMask & mProgramActiveAttribLocationsMask;
            for (auto attribIndex : candidateAttributesMask)
            {

                if (mForcedStreamingAttributesFirstOffsets[attribIndex] != first)
                {
                    updatedStreamingAttribsMask.set(attribIndex);
                    mForcedStreamingAttributesForDrawArraysInstancedMask.set(attribIndex);
                    mForcedStreamingAttributesFirstOffsets[attribIndex] = first;
                }
            }

            // We need to recover attributes whose divisor used to be > 0 but is reset to 0 now if
            // any
            auto forcedStreamingAttributesNeedRecoverMask =
                candidateAttributesMask ^ mForcedStreamingAttributesForDrawArraysInstancedMask;
            if (forcedStreamingAttributesNeedRecoverMask.any())
            {
                recoverForcedStreamingAttributesForDrawArraysInstanced(
                    context, &forcedStreamingAttributesNeedRecoverMask);
                mForcedStreamingAttributesForDrawArraysInstancedMask = candidateAttributesMask;
            }

            if (updatedStreamingAttribsMask.any())
            {
                ANGLE_TRY(streamAttributes(context, updatedStreamingAttribsMask, instanceCount,
                                           indexRange, true));
            }
            return angle::Result::Continue;
        }
    }

    if (needsStreamingAttribs.any())
    {
        ANGLE_TRY(
            streamAttributes(context, needsStreamingAttribs, instanceCount, indexRange, false));
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayGL::syncIndexData(const gl::Context *context,
                                           GLsizei count,
                                           gl::DrawElementsType type,
                                           const void *indices,
                                           bool primitiveRestartEnabled,
                                           bool attributesNeedStreaming,
                                           IndexRange *outIndexRange,
                                           const void **outIndices) const
{
    ASSERT(outIndices);

    gl::Buffer *elementArrayBuffer = mState.getElementArrayBuffer();

    // Need to check the range of indices if attributes need to be streamed
    if (elementArrayBuffer != nullptr)
    {
        ASSERT(elementArrayBuffer == mAppliedElementArrayBuffer.get());
        // Only compute the index range if the attributes also need to be streamed
        if (attributesNeedStreaming)
        {
            ptrdiff_t elementArrayBufferOffset = reinterpret_cast<ptrdiff_t>(indices);
            ANGLE_TRY(mState.getElementArrayBuffer()->getIndexRange(
                context, type, elementArrayBufferOffset, count, primitiveRestartEnabled,
                outIndexRange));
        }

        // Indices serves as an offset into the index buffer in this case, use the same value for
        // the draw call
        *outIndices = indices;
    }
    else
    {
        const FunctionsGL *functions = GetFunctionsGL(context);
        StateManagerGL *stateManager = GetStateManagerGL(context);

        // Need to stream the index buffer
        // TODO: if GLES, nothing needs to be streamed

        // Only compute the index range if the attributes also need to be streamed
        if (attributesNeedStreaming)
        {
            *outIndexRange = ComputeIndexRange(type, indices, count, primitiveRestartEnabled);
        }

        // Allocate the streaming element array buffer
        if (mStreamingElementArrayBuffer == 0)
        {
            functions->genBuffers(1, &mStreamingElementArrayBuffer);
            mStreamingElementArrayBufferSize = 0;
        }

        stateManager->bindVertexArray(mVertexArrayID, getAppliedElementArrayBufferID());

        stateManager->bindBuffer(gl::BufferBinding::ElementArray, mStreamingElementArrayBuffer);
        mAppliedElementArrayBuffer.set(context, nullptr);

        // Make sure the element array buffer is large enough
        const GLuint indexTypeBytes        = gl::GetDrawElementsTypeSize(type);
        size_t requiredStreamingBufferSize = indexTypeBytes * count;
        if (requiredStreamingBufferSize > mStreamingElementArrayBufferSize)
        {
            // Copy the indices in while resizing the buffer
            functions->bufferData(GL_ELEMENT_ARRAY_BUFFER, requiredStreamingBufferSize, indices,
                                  GL_DYNAMIC_DRAW);
            mStreamingElementArrayBufferSize = requiredStreamingBufferSize;
        }
        else
        {
            // Put the indices at the beginning of the buffer
            functions->bufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, requiredStreamingBufferSize,
                                     indices);
        }

        // Set the index offset for the draw call to zero since the supplied index pointer is to
        // client data
        *outIndices = nullptr;
    }

    return angle::Result::Continue;
}

void VertexArrayGL::computeStreamingAttributeSizes(const gl::AttributesMask &attribsToStream,
                                                   GLsizei instanceCount,
                                                   const gl::IndexRange &indexRange,
                                                   size_t *outStreamingDataSize,
                                                   size_t *outMaxAttributeDataSize) const
{
    *outStreamingDataSize    = 0;
    *outMaxAttributeDataSize = 0;

    ASSERT(attribsToStream.any());

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (auto idx : attribsToStream)
    {
        const auto &attrib  = attribs[idx];
        const auto &binding = bindings[attrib.bindingIndex];

        // If streaming is going to be required, compute the size of the required buffer
        // and how much slack space at the beginning of the buffer will be required by determining
        // the attribute with the largest data size.
        size_t typeSize        = ComputeVertexAttributeTypeSize(attrib);
        GLuint adjustedDivisor = GetAdjustedDivisor(mAppliedNumViews, binding.getDivisor());
        *outStreamingDataSize +=
            typeSize * ComputeVertexBindingElementCount(adjustedDivisor, indexRange.vertexCount(),
                                                        instanceCount);
        *outMaxAttributeDataSize = std::max(*outMaxAttributeDataSize, typeSize);
    }
}

angle::Result VertexArrayGL::streamAttributes(
    const gl::Context *context,
    const gl::AttributesMask &attribsToStream,
    GLsizei instanceCount,
    const gl::IndexRange &indexRange,
    bool applyExtraOffsetWorkaroundForInstancedAttributes) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    // Sync the vertex attribute state and track what data needs to be streamed
    size_t streamingDataSize    = 0;
    size_t maxAttributeDataSize = 0;

    computeStreamingAttributeSizes(attribsToStream, instanceCount, indexRange, &streamingDataSize,
                                   &maxAttributeDataSize);

    if (streamingDataSize == 0)
    {
        return angle::Result::Continue;
    }

    if (mStreamingArrayBuffer == 0)
    {
        functions->genBuffers(1, &mStreamingArrayBuffer);
        mStreamingArrayBufferSize = 0;
    }

    // If first is greater than zero, a slack space needs to be left at the beginning of the buffer
    // for each attribute so that the same 'first' argument can be passed into the draw call.
    const size_t bufferEmptySpace =
        attribsToStream.count() * maxAttributeDataSize * indexRange.start;
    const size_t requiredBufferSize = streamingDataSize + bufferEmptySpace;

    stateManager->bindBuffer(gl::BufferBinding::Array, mStreamingArrayBuffer);
    if (requiredBufferSize > mStreamingArrayBufferSize)
    {
        functions->bufferData(GL_ARRAY_BUFFER, requiredBufferSize, nullptr, GL_DYNAMIC_DRAW);
        mStreamingArrayBufferSize = requiredBufferSize;
    }

    stateManager->bindVertexArray(mVertexArrayID, getAppliedElementArrayBufferID());

    // Unmapping a buffer can return GL_FALSE to indicate that the system has corrupted the data
    // somehow (such as by a screen change), retry writing the data a few times and return
    // OUT_OF_MEMORY if that fails.
    GLboolean unmapResult     = GL_FALSE;
    size_t unmapRetryAttempts = 5;
    while (unmapResult != GL_TRUE && --unmapRetryAttempts > 0)
    {
        uint8_t *bufferPointer = MapBufferRangeWithFallback(functions, GL_ARRAY_BUFFER, 0,
                                                            requiredBufferSize, GL_MAP_WRITE_BIT);
        size_t curBufferOffset = maxAttributeDataSize * indexRange.start;

        const auto &attribs  = mState.getVertexAttributes();
        const auto &bindings = mState.getVertexBindings();

        for (auto idx : attribsToStream)
        {
            const auto &attrib = attribs[idx];
            ASSERT(IsVertexAttribPointerSupported(idx, attrib));

            const auto &binding = bindings[attrib.bindingIndex];

            GLuint adjustedDivisor = GetAdjustedDivisor(mAppliedNumViews, binding.getDivisor());
            // streamedVertexCount is only going to be modified by
            // shiftInstancedArrayDataWithExtraOffset workaround, otherwise it's const
            size_t streamedVertexCount = ComputeVertexBindingElementCount(
                adjustedDivisor, indexRange.vertexCount(), instanceCount);

            const size_t sourceStride = ComputeVertexAttributeStride(attrib, binding);
            const size_t destStride   = ComputeVertexAttributeTypeSize(attrib);

            // Vertices do not apply the 'start' offset when the divisor is non-zero even when doing
            // a non-instanced draw call
            const size_t firstIndex =
                (adjustedDivisor == 0 || applyExtraOffsetWorkaroundForInstancedAttributes)
                    ? indexRange.start
                    : 0;

            // Attributes using client memory ignore the VERTEX_ATTRIB_BINDING state.
            // https://www.opengl.org/registry/specs/ARB/vertex_attrib_binding.txt
            const uint8_t *inputPointer = static_cast<const uint8_t *>(attrib.pointer);
            // store batchMemcpySize since streamedVertexCount could be changed by workaround
            const size_t batchMemcpySize = destStride * streamedVertexCount;

            size_t batchMemcpyInputOffset                    = sourceStride * firstIndex;
            bool needsUnmapAndRebindStreamingAttributeBuffer = false;
            size_t firstIndexForSeparateCopy                 = firstIndex;

            if (applyExtraOffsetWorkaroundForInstancedAttributes && adjustedDivisor > 0)
            {
                const size_t originalStreamedVertexCount = streamedVertexCount;
                streamedVertexCount =
                    (instanceCount + indexRange.start + adjustedDivisor - 1u) / adjustedDivisor;

                const size_t copySize =
                    sourceStride *
                    originalStreamedVertexCount;  // the real data in the buffer we are streaming

                const gl::Buffer *bindingBufferPointer = binding.getBuffer().get();
                if (!bindingBufferPointer)
                {
                    if (!inputPointer)
                    {
                        continue;
                    }
                    inputPointer = static_cast<const uint8_t *>(attrib.pointer);
                }
                else
                {
                    needsUnmapAndRebindStreamingAttributeBuffer = true;
                    const auto buffer = GetImplAs<BufferGL>(bindingBufferPointer);
                    stateManager->bindBuffer(gl::BufferBinding::Array, buffer->getBufferID());
                    // The workaround is only for latest Mac Intel so glMapBufferRange should be
                    // supported
                    ASSERT(CanMapBufferForRead(functions));
                    uint8_t *inputBufferPointer = MapBufferRangeWithFallback(
                        functions, GL_ARRAY_BUFFER, binding.getOffset(), copySize, GL_MAP_READ_BIT);
                    ASSERT(inputBufferPointer);
                    inputPointer = inputBufferPointer;
                }

                batchMemcpyInputOffset    = 0;
                firstIndexForSeparateCopy = 0;
            }

            // Pack the data when copying it, user could have supplied a very large stride that
            // would cause the buffer to be much larger than needed.
            if (destStride == sourceStride)
            {
                // Can copy in one go, the data is packed
                memcpy(bufferPointer + curBufferOffset, inputPointer + batchMemcpyInputOffset,
                       batchMemcpySize);
            }
            else
            {
                for (size_t vertexIdx = 0; vertexIdx < streamedVertexCount; vertexIdx++)
                {
                    uint8_t *out = bufferPointer + curBufferOffset + (destStride * vertexIdx);
                    const uint8_t *in =
                        inputPointer + sourceStride * (vertexIdx + firstIndexForSeparateCopy);
                    memcpy(out, in, destStride);
                }
            }

            if (needsUnmapAndRebindStreamingAttributeBuffer)
            {
                ANGLE_GL_TRY(context, functions->unmapBuffer(GL_ARRAY_BUFFER));
                stateManager->bindBuffer(gl::BufferBinding::Array, mStreamingArrayBuffer);
            }

            // Compute where the 0-index vertex would be.
            const size_t vertexStartOffset = curBufferOffset - (firstIndex * destStride);

            callVertexAttribPointer(context, static_cast<GLuint>(idx), attrib,
                                    static_cast<GLsizei>(destStride),
                                    static_cast<GLintptr>(vertexStartOffset));

            // Update the state to track the streamed attribute
            mAppliedAttributes[idx].format = attrib.format;

            mAppliedAttributes[idx].relativeOffset = 0;
            mAppliedAttributes[idx].bindingIndex   = static_cast<GLuint>(idx);

            mAppliedBindings[idx].setStride(static_cast<GLsizei>(destStride));
            mAppliedBindings[idx].setOffset(static_cast<GLintptr>(vertexStartOffset));
            mAppliedBindings[idx].setBuffer(context, nullptr);

            // There's maxAttributeDataSize * indexRange.start of empty space allocated for each
            // streaming attributes
            curBufferOffset +=
                destStride * streamedVertexCount + maxAttributeDataSize * indexRange.start;
        }

        unmapResult = functions->unmapBuffer(GL_ARRAY_BUFFER);
    }

    ANGLE_CHECK(GetImplAs<ContextGL>(context), unmapResult == GL_TRUE,
                "Failed to unmap the client data streaming buffer.", GL_OUT_OF_MEMORY);
    return angle::Result::Continue;
}

void VertexArrayGL::recoverForcedStreamingAttributesForDrawArraysInstanced(
    const gl::Context *context) const
{
    recoverForcedStreamingAttributesForDrawArraysInstanced(
        context, &mForcedStreamingAttributesForDrawArraysInstancedMask);
}

void VertexArrayGL::recoverForcedStreamingAttributesForDrawArraysInstanced(
    const gl::Context *context,
    gl::AttributesMask *attributeMask) const
{
    if (attributeMask->none())
    {
        return;
    }

    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindVertexArray(mVertexArrayID, getAppliedElementArrayBufferID());

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();
    for (auto idx : *attributeMask)
    {
        const auto &attrib = attribs[idx];
        ASSERT(IsVertexAttribPointerSupported(idx, attrib));

        const auto &binding = bindings[attrib.bindingIndex];
        const auto buffer   = GetImplAs<BufferGL>(binding.getBuffer().get());
        stateManager->bindBuffer(gl::BufferBinding::Array, buffer->getBufferID());

        callVertexAttribPointer(context, static_cast<GLuint>(idx), attrib,
                                static_cast<GLsizei>(binding.getStride()),
                                static_cast<GLintptr>(binding.getOffset()));

        // Restore the state to track their original buffers
        mAppliedAttributes[idx].format = attrib.format;

        mAppliedAttributes[idx].relativeOffset = 0;
        mAppliedAttributes[idx].bindingIndex   = static_cast<GLuint>(attrib.bindingIndex);

        mAppliedBindings[idx].setStride(binding.getStride());
        mAppliedBindings[idx].setOffset(binding.getOffset());
        mAppliedBindings[idx].setBuffer(context, binding.getBuffer().get());
    }

    attributeMask->reset();
    mForcedStreamingAttributesFirstOffsets.fill(0);
}

GLuint VertexArrayGL::getVertexArrayID() const
{
    return mVertexArrayID;
}

GLuint VertexArrayGL::getAppliedElementArrayBufferID() const
{
    if (mAppliedElementArrayBuffer.get() == nullptr)
    {
        return mStreamingElementArrayBuffer;
    }

    return GetImplAs<BufferGL>(mAppliedElementArrayBuffer.get())->getBufferID();
}

void VertexArrayGL::updateAttribEnabled(const gl::Context *context, size_t attribIndex)
{
    const bool enabled = mState.getVertexAttribute(attribIndex).enabled &
                         mProgramActiveAttribLocationsMask.test(attribIndex);
    if (mAppliedAttributes[attribIndex].enabled == enabled)
    {
        return;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);

    if (enabled)
    {
        functions->enableVertexAttribArray(static_cast<GLuint>(attribIndex));
    }
    else
    {
        functions->disableVertexAttribArray(static_cast<GLuint>(attribIndex));
    }

    mAppliedAttributes[attribIndex].enabled = enabled;
}

void VertexArrayGL::updateAttribPointer(const gl::Context *context, size_t attribIndex)
{

    const VertexAttribute &attrib = mState.getVertexAttribute(attribIndex);

    // According to SPEC, VertexAttribPointer should update the binding indexed attribIndex instead
    // of the binding indexed attrib.bindingIndex (unless attribIndex == attrib.bindingIndex).
    const VertexBinding &binding = mState.getVertexBinding(attribIndex);

    // Early return when the vertex attribute isn't using a buffer object:
    // - If we need to stream, defer the attribPointer to the draw call.
    // - Skip the attribute that is disabled and uses a client memory pointer.
    // - Skip the attribute whose buffer is detached by BindVertexBuffer. Since it cannot have a
    //   client memory pointer either, it must be disabled and shouldn't affect the draw.
    const auto &bindingBuffer = binding.getBuffer();
    const Buffer *arrayBuffer = bindingBuffer.get();
    if (arrayBuffer == nullptr)
    {
        // Mark the applied binding isn't using a buffer by setting its buffer to nullptr so that if
        // it starts to use a buffer later, there is no chance that the caching will skip it.
        mAppliedBindings[attribIndex].setBuffer(context, nullptr);
        return;
    }

    // We do not need to compare attrib.pointer because when we use a different client memory
    // pointer, we don't need to update mAttributesNeedStreaming by binding.buffer and we won't
    // update attribPointer in this function.
    if ((SameVertexAttribFormat(mAppliedAttributes[attribIndex], attrib)) &&
        (mAppliedAttributes[attribIndex].bindingIndex == attrib.bindingIndex) &&
        (SameVertexBuffer(mAppliedBindings[attribIndex], binding)))
    {
        return;
    }

    // Since ANGLE always uses a non-zero VAO, we cannot use a client memory pointer on it:
    // [OpenGL ES 3.0.2] Section 2.8 page 24:
    // An INVALID_OPERATION error is generated when a non-zero vertex array object is bound,
    // zero is bound to the ARRAY_BUFFER buffer object binding point, and the pointer argument
    // is not NULL.

    StateManagerGL *stateManager  = GetStateManagerGL(context);
    const BufferGL *arrayBufferGL = GetImplAs<BufferGL>(arrayBuffer);
    stateManager->bindBuffer(gl::BufferBinding::Array, arrayBufferGL->getBufferID());
    callVertexAttribPointer(context, static_cast<GLuint>(attribIndex), attrib, binding.getStride(),
                            binding.getOffset());

    mAppliedAttributes[attribIndex].format = attrib.format;

    // After VertexAttribPointer, attrib.relativeOffset is set to 0 and attrib.bindingIndex is set
    // to attribIndex in driver. If attrib.relativeOffset != 0 or attrib.bindingIndex !=
    // attribIndex, they should be set in updateAttribFormat and updateAttribBinding. The cache
    // should be consistent with driver so that we won't miss anything.
    mAppliedAttributes[attribIndex].relativeOffset = 0;
    mAppliedAttributes[attribIndex].bindingIndex   = static_cast<GLuint>(attribIndex);

    mAppliedBindings[attribIndex].setStride(binding.getStride());
    mAppliedBindings[attribIndex].setOffset(binding.getOffset());
    mAppliedBindings[attribIndex].setBuffer(context, binding.getBuffer().get());
}

void VertexArrayGL::callVertexAttribPointer(const gl::Context *context,
                                            GLuint attribIndex,
                                            const VertexAttribute &attrib,
                                            GLsizei stride,
                                            GLintptr offset) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    const GLvoid *pointer        = reinterpret_cast<const GLvoid *>(offset);
    const angle::Format &format  = *attrib.format;
    if (format.isPureInt())
    {
        ASSERT(!format.isNorm());
        functions->vertexAttribIPointer(attribIndex, format.channelCount,
                                        gl::ToGLenum(format.vertexAttribType), stride, pointer);
    }
    else
    {
        functions->vertexAttribPointer(attribIndex, format.channelCount,
                                       gl::ToGLenum(format.vertexAttribType), format.isNorm(),
                                       stride, pointer);
    }
}

bool VertexArrayGL::supportVertexAttribBinding(const gl::Context *context) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    ASSERT(functions);
    return (functions->vertexAttribBinding != nullptr);
}

void VertexArrayGL::updateAttribFormat(const gl::Context *context, size_t attribIndex)
{
    ASSERT(supportVertexAttribBinding(context));

    const VertexAttribute &attrib = mState.getVertexAttribute(attribIndex);
    if (SameVertexAttribFormat(mAppliedAttributes[attribIndex], attrib))
    {
        return;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);

    const angle::Format &format = *attrib.format;
    if (format.isPureInt())
    {
        ASSERT(!format.isNorm());
        functions->vertexAttribIFormat(static_cast<GLuint>(attribIndex), format.channelCount,
                                       gl::ToGLenum(format.vertexAttribType),
                                       attrib.relativeOffset);
    }
    else
    {
        functions->vertexAttribFormat(static_cast<GLuint>(attribIndex), format.channelCount,
                                      gl::ToGLenum(format.vertexAttribType), format.isNorm(),
                                      attrib.relativeOffset);
    }

    mAppliedAttributes[attribIndex].format         = attrib.format;
    mAppliedAttributes[attribIndex].relativeOffset = attrib.relativeOffset;
}

void VertexArrayGL::updateAttribBinding(const gl::Context *context, size_t attribIndex)
{
    ASSERT(supportVertexAttribBinding(context));

    GLuint bindingIndex = mState.getVertexAttribute(attribIndex).bindingIndex;
    if (mAppliedAttributes[attribIndex].bindingIndex == bindingIndex)
    {
        return;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    functions->vertexAttribBinding(static_cast<GLuint>(attribIndex), bindingIndex);

    mAppliedAttributes[attribIndex].bindingIndex = bindingIndex;
}

void VertexArrayGL::updateBindingBuffer(const gl::Context *context, size_t bindingIndex)
{
    ASSERT(supportVertexAttribBinding(context));

    const VertexBinding &binding = mState.getVertexBinding(bindingIndex);
    if (SameVertexBuffer(mAppliedBindings[bindingIndex], binding))
    {
        return;
    }

    const Buffer *arrayBuffer = binding.getBuffer().get();
    GLuint bufferId           = 0;
    if (arrayBuffer != nullptr)
    {
        bufferId = GetImplAs<BufferGL>(arrayBuffer)->getBufferID();
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    functions->bindVertexBuffer(static_cast<GLuint>(bindingIndex), bufferId, binding.getOffset(),
                                binding.getStride());

    mAppliedBindings[bindingIndex].setStride(binding.getStride());
    mAppliedBindings[bindingIndex].setOffset(binding.getOffset());
    mAppliedBindings[bindingIndex].setBuffer(context, binding.getBuffer().get());
}

void VertexArrayGL::updateBindingDivisor(const gl::Context *context, size_t bindingIndex)
{
    GLuint adjustedDivisor =
        GetAdjustedDivisor(mAppliedNumViews, mState.getVertexBinding(bindingIndex).getDivisor());
    if (mAppliedBindings[bindingIndex].getDivisor() == adjustedDivisor)
    {
        return;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    if (supportVertexAttribBinding(context))
    {
        functions->vertexBindingDivisor(static_cast<GLuint>(bindingIndex), adjustedDivisor);
    }
    else
    {
        // We can only use VertexAttribDivisor on platforms that don't support Vertex Attrib
        // Binding.
        functions->vertexAttribDivisor(static_cast<GLuint>(bindingIndex), adjustedDivisor);
    }

    mAppliedBindings[bindingIndex].setDivisor(adjustedDivisor);

    if (adjustedDivisor > 0)
    {
        mInstancedAttributesMask.set(bindingIndex);
    }
    else if (mInstancedAttributesMask.test(bindingIndex))
    {
        // divisor is reset to 0
        mInstancedAttributesMask.reset(bindingIndex);
    }
}

void VertexArrayGL::syncDirtyAttrib(const gl::Context *context,
                                    size_t attribIndex,
                                    const gl::VertexArray::DirtyAttribBits &dirtyAttribBits)
{
    ASSERT(dirtyAttribBits.any());

    for (size_t dirtyBit : dirtyAttribBits)
    {
        switch (dirtyBit)
        {
            case VertexArray::DIRTY_ATTRIB_ENABLED:
                updateAttribEnabled(context, attribIndex);
                break;

            case VertexArray::DIRTY_ATTRIB_POINTER_BUFFER:
            case VertexArray::DIRTY_ATTRIB_POINTER:
                updateAttribPointer(context, attribIndex);
                break;

            case VertexArray::DIRTY_ATTRIB_FORMAT:
                ASSERT(supportVertexAttribBinding(context));
                updateAttribFormat(context, attribIndex);
                break;

            case VertexArray::DIRTY_ATTRIB_BINDING:
                ASSERT(supportVertexAttribBinding(context));
                updateAttribBinding(context, attribIndex);
                break;

            default:
                UNREACHABLE();
                break;
        }
    }
}

void VertexArrayGL::syncDirtyBinding(const gl::Context *context,
                                     size_t bindingIndex,
                                     const gl::VertexArray::DirtyBindingBits &dirtyBindingBits)
{
    // Dependent state changes in buffers can trigger updates with no dirty bits set.

    for (size_t dirtyBit : dirtyBindingBits)
    {
        switch (dirtyBit)
        {
            case VertexArray::DIRTY_BINDING_BUFFER:
                ASSERT(supportVertexAttribBinding(context));
                updateBindingBuffer(context, bindingIndex);
                break;

            case VertexArray::DIRTY_BINDING_DIVISOR:
                updateBindingDivisor(context, bindingIndex);
                break;

            default:
                UNREACHABLE();
                break;
        }
    }
}

#define ANGLE_DIRTY_ATTRIB_FUNC(INDEX)                         \
    case VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:              \
        syncDirtyAttrib(context, INDEX, (*attribBits)[INDEX]); \
        (*attribBits)[INDEX].reset();                          \
        break;

#define ANGLE_DIRTY_BINDING_FUNC(INDEX)                          \
    case VertexArray::DIRTY_BIT_BINDING_0 + INDEX:               \
        syncDirtyBinding(context, INDEX, (*bindingBits)[INDEX]); \
        (*bindingBits)[INDEX].reset();                           \
        break;

#define ANGLE_DIRTY_BUFFER_DATA_FUNC(INDEX)            \
    case VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX: \
        break;

angle::Result VertexArrayGL::syncState(const gl::Context *context,
                                       const gl::VertexArray::DirtyBits &dirtyBits,
                                       gl::VertexArray::DirtyAttribBitsArray *attribBits,
                                       gl::VertexArray::DirtyBindingBitsArray *bindingBits)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);
    stateManager->bindVertexArray(mVertexArrayID, getAppliedElementArrayBufferID());

    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
                updateElementArrayBufferBinding(context);
                break;

            case VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
                break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_DIRTY_ATTRIB_FUNC)
                ANGLE_VERTEX_INDEX_CASES(ANGLE_DIRTY_BINDING_FUNC)
                ANGLE_VERTEX_INDEX_CASES(ANGLE_DIRTY_BUFFER_DATA_FUNC)

            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

void VertexArrayGL::applyNumViewsToDivisor(const gl::Context *context, int numViews)
{
    if (numViews != mAppliedNumViews)
    {
        StateManagerGL *stateManager = GetStateManagerGL(context);
        stateManager->bindVertexArray(mVertexArrayID, getAppliedElementArrayBufferID());
        mAppliedNumViews = numViews;
        for (size_t index = 0u; index < mAppliedBindings.size(); ++index)
        {
            updateBindingDivisor(context, index);
        }
    }
}

void VertexArrayGL::applyActiveAttribLocationsMask(const gl::Context *context,
                                                   const gl::AttributesMask &activeMask)
{
    gl::AttributesMask updateMask = mProgramActiveAttribLocationsMask ^ activeMask;
    if (!updateMask.any())
    {
        return;
    }

    ASSERT(mVertexArrayID == GetStateManagerGL(context)->getVertexArrayID());
    mProgramActiveAttribLocationsMask = activeMask;

    for (size_t attribIndex : updateMask)
    {
        updateAttribEnabled(context, attribIndex);
    }
}

void VertexArrayGL::validateState(const gl::Context *context) const
{
    const FunctionsGL *functions = GetFunctionsGL(context);

    // Ensure this vao is currently bound
    ValidateStateHelperGetIntegerv(functions, mVertexArrayID, GL_VERTEX_ARRAY_BINDING,
                                   "mVertexArrayID", "GL_VERTEX_ARRAY_BINDING");

    // Element array buffer
    if (mAppliedElementArrayBuffer.get() == nullptr)
    {
        ValidateStateHelperGetIntegerv(
            functions, mStreamingElementArrayBuffer, GL_ELEMENT_ARRAY_BUFFER_BINDING,
            "mAppliedElementArrayBuffer", "GL_ELEMENT_ARRAY_BUFFER_BINDING");
    }
    else
    {
        const BufferGL *bufferGL = GetImplAs<BufferGL>(mAppliedElementArrayBuffer.get());
        ValidateStateHelperGetIntegerv(
            functions, bufferGL->getBufferID(), GL_ELEMENT_ARRAY_BUFFER_BINDING,
            "mAppliedElementArrayBuffer", "GL_ELEMENT_ARRAY_BUFFER_BINDING");
    }

    // ValidateStateHelperGetIntegerv but with > comparison instead of !=
    GLint queryValue;
    functions->getIntegerv(GL_MAX_VERTEX_ATTRIBS, &queryValue);
    if (mAppliedAttributes.size() > static_cast<GLuint>(queryValue))
    {
        WARN() << "mAppliedAttributes.size() (" << mAppliedAttributes.size()
               << ") > GL_MAX_VERTEX_ATTRIBS (" << queryValue << ")";
        // Re-add ASSERT: http://anglebug.com/3900
        // ASSERT(false);
    }

    // Check each applied attribute/binding
    for (GLuint index = 0; index < mAppliedAttributes.size(); index++)
    {
        VertexAttribute &attribute = mAppliedAttributes[index];
        ASSERT(attribute.bindingIndex < mAppliedBindings.size());
        VertexBinding &binding = mAppliedBindings[attribute.bindingIndex];

        ValidateStateHelperGetVertexAttribiv(
            functions, index, attribute.enabled, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
            "mAppliedAttributes.enabled", "GL_VERTEX_ATTRIB_ARRAY_ENABLED");

        if (attribute.enabled)
        {
            // Applied attributes
            ASSERT(attribute.format);
            ValidateStateHelperGetVertexAttribiv(
                functions, index, ToGLenum(attribute.format->vertexAttribType),
                GL_VERTEX_ATTRIB_ARRAY_TYPE, "mAppliedAttributes.format->vertexAttribType",
                "GL_VERTEX_ATTRIB_ARRAY_TYPE");
            ValidateStateHelperGetVertexAttribiv(
                functions, index, attribute.format->channelCount, GL_VERTEX_ATTRIB_ARRAY_SIZE,
                "attribute.format->channelCount", "GL_VERTEX_ATTRIB_ARRAY_SIZE");
            ValidateStateHelperGetVertexAttribiv(
                functions, index, attribute.format->isNorm(), GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                "attribute.format->isNorm()", "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED");
            ValidateStateHelperGetVertexAttribiv(
                functions, index, attribute.format->isPureInt(), GL_VERTEX_ATTRIB_ARRAY_INTEGER,
                "attribute.format->isPureInt()", "GL_VERTEX_ATTRIB_ARRAY_INTEGER");
            if (supportVertexAttribBinding(context))
            {
                ValidateStateHelperGetVertexAttribiv(
                    functions, index, attribute.relativeOffset, GL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                    "attribute.relativeOffset", "GL_VERTEX_ATTRIB_RELATIVE_OFFSET");
                ValidateStateHelperGetVertexAttribiv(
                    functions, index, attribute.bindingIndex, GL_VERTEX_ATTRIB_BINDING,
                    "attribute.bindingIndex", "GL_VERTEX_ATTRIB_BINDING");
            }

            // Applied bindings
            if (binding.getBuffer().get() == nullptr)
            {
                ValidateStateHelperGetVertexAttribiv(
                    functions, index, mStreamingArrayBuffer, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                    "mAppliedBindings.bufferID", "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING");
            }
            else
            {
                const BufferGL *arrayBufferGL = GetImplAs<BufferGL>(binding.getBuffer().get());
                ASSERT(arrayBufferGL);
                ValidateStateHelperGetVertexAttribiv(functions, index, arrayBufferGL->getBufferID(),
                                                     GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                                                     "mAppliedBindings.bufferID",
                                                     "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING");
                ValidateStateHelperGetVertexAttribiv(
                    functions, index, binding.getStride(), GL_VERTEX_ATTRIB_ARRAY_STRIDE,
                    "binding.getStride()", "GL_VERTEX_ATTRIB_ARRAY_STRIDE");
                ValidateStateHelperGetVertexAttribiv(
                    functions, index, binding.getDivisor(), GL_VERTEX_ATTRIB_ARRAY_DIVISOR,
                    "binding.getDivisor()", "GL_VERTEX_ATTRIB_ARRAY_DIVISOR");
            }
        }
    }
}

}  // namespace rx
