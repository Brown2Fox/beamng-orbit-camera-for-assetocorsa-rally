#pragma once

namespace BeamNGOrbitCamera::MiniMath
{
    inline constexpr double Pi = 3.14159265358979323846;
    inline constexpr double DegToRad = Pi / 180.0;
    inline constexpr double RadToDeg = 180.0 / Pi;
    inline constexpr double Epsilon = 1e-9;

    struct FVector
    {
        double X{};
        double Y{};
        double Z{};

        FVector operator+(const FVector& Other) const
        {
            return {X + Other.X, Y + Other.Y, Z + Other.Z};
        }

        FVector operator-(const FVector& Other) const
        {
            return {X - Other.X, Y - Other.Y, Z - Other.Z};
        }

        FVector operator*(double Scalar) const
        {
            return {X * Scalar, Y * Scalar, Z * Scalar};
        }

        FVector operator/(double Scalar) const
        {
            return {X / Scalar, Y / Scalar, Z / Scalar};
        }
    };

    struct FRotator
    {
        double Pitch{};
        double Yaw{};
        double Roll{};
    };

    struct FMatrix3
    {
        double M[3][3]{};
    };

    FVector WorldUp();

    double Dot(const FVector& A, const FVector& B);

    FVector Cross(const FVector& A, const FVector& B);

    double Length(const FVector& Vector);

    FVector Normalized(const FVector& Vector, const FVector& Fallback);

    FVector PlanarHeading(const FVector& Vector);

    double WrapRadians(double Angle);
    double WrapDegrees(double Angle);

    double ShortestAngleDifference(double Current, double Target);

    double SignedHeadingError(const FVector& From, const FVector& To);

    FVector RotateAroundAxis(const FVector& Vector, const FVector& Axis, double Angle);

    void RateAccelStep(double& Current, double& Velocity, double Target, double Rate, double Acceleration, double DeltaTimeSeconds);

    double VerticalToHorizontalFovDeg(double VerticalFovDeg, double AspectRatio);

    FMatrix3 RotatorToMatrix(const FRotator& Rotator);

    FVector MatrixVehicleForward(const FMatrix3& Matrix);

    FVector MatrixVehicleUp(const FMatrix3& Matrix);

    FVector Rotate(const FMatrix3& Matrix, const FVector& Vector);

    FRotator DirectionToRotator(const FVector& Direction);
}
