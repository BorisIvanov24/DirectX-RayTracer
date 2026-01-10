#include "CRTMatrix.h"
#include <iostream>

CRTMatrix operator*(const CRTMatrix& lhs, const CRTMatrix& rhs)
{
    CRTMatrix ans;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            ans.m[i][j] = 0.0f;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int p = 0; p < 3; p++)
            {
                ans.m[i][j] += (lhs.m[i][p] * rhs.m[p][j]);
            }
        }
    }

    return ans;
}

CRTVector operator*(const CRTVector& lhs, const CRTMatrix& rhs)
{
    float newVec[3] = { 0.f };
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            newVec[i] += (lhs.getByIndex(j) * rhs.m[j][i]);
        }
    }

    return CRTVector(newVec[0], newVec[1], newVec[2]);
}

CRTMatrix::CRTMatrix() : CRTMatrix(1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f)
{
}

CRTMatrix::CRTMatrix(float c00, float c01, float c02, float c10, float c11, float c12, float c20, float c21, float c22)
{
    m[0][0] = c00;
    m[0][1] = c01;
    m[0][2] = c02;

    m[1][0] = c10;
    m[1][1] = c11;
    m[1][2] = c12;

    m[2][0] = c20;
    m[2][1] = c21;
    m[2][2] = c22;
}

void CRTMatrix::print() const
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            std::cout << m[i][j] << ' ';
        }
        std::cout << std::endl;
    }
}
