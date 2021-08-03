//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Scalarize vector and matrix constructor args, so that vectors built from components don't have
// matrix arguments, and matrices built from components don't have vector arguments. This avoids
// driver bugs around vector and matrix constructors.
//

#include "compiler/translator/tree_ops/ScalarizeVecAndMatConstructorArgs.h"
#include "common/debug.h"

#include <algorithm>

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

TIntermBinary *ConstructVectorIndexBinaryNode(TIntermSymbol *symbolNode, int index)
{
    return new TIntermBinary(EOpIndexDirect, symbolNode, CreateIndexNode(index));
}

TIntermBinary *ConstructMatrixIndexBinaryNode(TIntermSymbol *symbolNode, int colIndex, int rowIndex)
{
    TIntermBinary *colVectorNode = ConstructVectorIndexBinaryNode(symbolNode, colIndex);

    return new TIntermBinary(EOpIndexDirect, colVectorNode, CreateIndexNode(rowIndex));
}

class ScalarizeArgsTraverser : public TIntermTraverser
{
  public:
    ScalarizeArgsTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable),
          mNodesToScalarize(IntermNodePatternMatcher::kScalarizedVecOrMatConstructor)
    {}

  protected:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;

  private:
    void scalarizeArgs(TIntermAggregate *aggregate, bool scalarizeVector, bool scalarizeMatrix);

    // If we have the following code:
    //   mat4 m(0);
    //   vec4 v(1, m);
    // We will rewrite to:
    //   mat4 m(0);
    //   mat4 s0 = m;
    //   vec4 v(1, s0[0][0], s0[0][1], s0[0][2]);
    // This function is to create nodes for "mat4 s0 = m;" and insert it to the code sequence. This
    // way the possible side effects of the constructor argument will only be evaluated once.
    TVariable *createTempVariable(TIntermTyped *original);

    std::vector<TIntermSequence> mBlockStack;

    IntermNodePatternMatcher mNodesToScalarize;
};

bool ScalarizeArgsTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    ASSERT(visit == PreVisit);
    if (mNodesToScalarize.match(node, getParentNode()))
    {
        if (node->getType().isVector())
        {
            scalarizeArgs(node, false, true);
        }
        else
        {
            ASSERT(node->getType().isMatrix());
            scalarizeArgs(node, true, false);
        }
    }
    return true;
}

bool ScalarizeArgsTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    mBlockStack.push_back(TIntermSequence());
    {
        for (TIntermNode *child : *node->getSequence())
        {
            ASSERT(child != nullptr);
            child->traverse(this);
            mBlockStack.back().push_back(child);
        }
    }
    if (mBlockStack.back().size() > node->getSequence()->size())
    {
        node->getSequence()->clear();
        *(node->getSequence()) = mBlockStack.back();
    }
    mBlockStack.pop_back();
    return false;
}

void ScalarizeArgsTraverser::scalarizeArgs(TIntermAggregate *aggregate,
                                           bool scalarizeVector,
                                           bool scalarizeMatrix)
{
    ASSERT(aggregate);
    ASSERT(!aggregate->isArray());
    int size                  = static_cast<int>(aggregate->getType().getObjectSize());
    TIntermSequence *sequence = aggregate->getSequence();
    TIntermSequence originalArgs(*sequence);
    sequence->clear();
    for (TIntermNode *originalArgNode : originalArgs)
    {
        ASSERT(size > 0);
        TIntermTyped *originalArg = originalArgNode->getAsTyped();
        ASSERT(originalArg);
        TVariable *argVariable = createTempVariable(originalArg);
        if (originalArg->isScalar())
        {
            sequence->push_back(CreateTempSymbolNode(argVariable));
            size--;
        }
        else if (originalArg->isVector())
        {
            if (scalarizeVector)
            {
                int repeat = std::min(size, originalArg->getNominalSize());
                size -= repeat;
                for (int index = 0; index < repeat; ++index)
                {
                    TIntermSymbol *symbolNode = CreateTempSymbolNode(argVariable);
                    TIntermBinary *newNode    = ConstructVectorIndexBinaryNode(symbolNode, index);
                    sequence->push_back(newNode);
                }
            }
            else
            {
                TIntermSymbol *symbolNode = CreateTempSymbolNode(argVariable);
                sequence->push_back(symbolNode);
                size -= originalArg->getNominalSize();
            }
        }
        else
        {
            ASSERT(originalArg->isMatrix());
            if (scalarizeMatrix)
            {
                int colIndex = 0, rowIndex = 0;
                int repeat = std::min(size, originalArg->getCols() * originalArg->getRows());
                size -= repeat;
                while (repeat > 0)
                {
                    TIntermSymbol *symbolNode = CreateTempSymbolNode(argVariable);
                    TIntermBinary *newNode =
                        ConstructMatrixIndexBinaryNode(symbolNode, colIndex, rowIndex);
                    sequence->push_back(newNode);
                    rowIndex++;
                    if (rowIndex >= originalArg->getRows())
                    {
                        rowIndex = 0;
                        colIndex++;
                    }
                    repeat--;
                }
            }
            else
            {
                TIntermSymbol *symbolNode = CreateTempSymbolNode(argVariable);
                sequence->push_back(symbolNode);
                size -= originalArg->getCols() * originalArg->getRows();
            }
        }
    }
}

TVariable *ScalarizeArgsTraverser::createTempVariable(TIntermTyped *original)
{
    ASSERT(original);

    TType *type = new TType(original->getType());
    type->setQualifier(EvqTemporary);

    TVariable *variable = CreateTempVariable(mSymbolTable, type);

    ASSERT(mBlockStack.size() > 0);
    TIntermSequence &sequence       = mBlockStack.back();
    TIntermDeclaration *declaration = CreateTempInitDeclarationNode(variable, original);
    sequence.push_back(declaration);

    return variable;
}

}  // namespace

bool ScalarizeVecAndMatConstructorArgs(TCompiler *compiler,
                                       TIntermBlock *root,
                                       TSymbolTable *symbolTable)
{
    ScalarizeArgsTraverser scalarizer(symbolTable);
    root->traverse(&scalarizer);

    return compiler->validateAST(root);
}

}  // namespace sh
