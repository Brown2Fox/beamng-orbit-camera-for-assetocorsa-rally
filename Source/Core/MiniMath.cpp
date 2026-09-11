#include "Core/MiniMath.hpp"

#include <algorithm>
#include <cmath>

namespace BeamNGOrbitCamera::MiniMath
{
    FVector WorldUp()
    {
        return {0.0, 0.0, 1.0};
    }

    double Dot(const FVector& A, const FVector& B)
    {
        return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
    }

    FVector Cross(const FVector& A, const FVector& B)
    {
        return {
            A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X
        };
    }

    double Length(const FVector& Vector)
    {
        return std::sqrt(Dot(Vector, Vector));
    }

    FVector Normalized(const FVector& Vector, const FVector& Fallback)
    {
        const double VectorLength = Length(Vector);

        return VectorLength > 0.0001
                ? Vector / VectorLength : Fallback;
    }

    FVector PlanarHeading(const FVector& Vector)
    {
        const FVector Heading{
            Vector.X, Vector.Y, 0.0
        };

        const double HeadingLength = Length(Heading);

        return HeadingLength > 0.0001
                ? Heading / HeadingLength : FVector{};
    }

    double WrapRadians(double Angle)
    {
        Angle = std::fmod(Angle + Pi, 2.0 * Pi);

        if (Angle < 0.0)
        {
            Angle += 2.0 * Pi;
        }

        return Angle - Pi;
    }

    double WrapDegrees(double Angle)
    {
        Angle = std::fmod(Angle + 180.0, 360.0);

        if (Angle < 0.0)
        {
            Angle += 360.0;
        }

        return Angle - 180.0;
    }

    double ShortestAngleDifference(double Current, double Target)
    {
        return WrapRadians(Target - Current);
    }

    double SignedHeadingError(const FVector& From, const FVector& To)
    {
        return std::atan2(Dot(Cross(From, To), WorldUp()), std::clamp(Dot(From, To), -1.0, 1.0));
    }

    FVector RotateAroundAxis(const FVector& Vector, const FVector& Axis, double Angle)
    {
        const FVector Normal = Normalized(Axis, WorldUp());

        const double Cosine = std::cos(Angle);

        const double Sine = std::sin(Angle);

        return Vector * Cosine + Cross(Normal, Vector) * Sine + Normal * (Dot(Normal, Vector) * (1.0 - Cosine));
    }

    namespace
    {
        double Sign(double Value)
        {
            return static_cast<double>((Value > 0.0) - (Value < 0.0));
        }
    }

    double TemporalSigmoidGet(double& State, double& PreviousVelocity, double Sample, double DeltaTimeSeconds,
        double InRate, double StartAcceleration, double StopAcceleration, double OutRate)
    {
        if (DeltaTimeSeconds <= 0.0)
            return State;

        const double Difference = Sample - State;
        const double PreviousVelocityInDirection = PreviousVelocity
            * std::max(Sign(PreviousVelocity * Difference), 0.0);
        const double VelocitySquared = PreviousVelocityInDirection * PreviousVelocityInDirection;
        const double AbsoluteDifference = std::abs(Difference);
        const double DifferenceSign = Sign(Difference);

        double AccelerationDeltaTime = 0.0;
        const double TwiceAbsoluteDifference = AbsoluteDifference * 2.0;
        if (VelocitySquared > TwiceAbsoluteDifference * StopAcceleration && TwiceAbsoluteDifference > 0.0)
        {
            AccelerationDeltaTime = -DifferenceSign
                * std::min((VelocitySquared / TwiceAbsoluteDifference) * DeltaTimeSeconds,
                    std::abs(PreviousVelocityInDirection));
        }
        else
        {
            AccelerationDeltaTime = DifferenceSign * StartAcceleration * DeltaTimeSeconds;
        }

        const double RateLimit = Difference * State >= 0.0 ? OutRate : InRate;
        State += DifferenceSign
            * std::min(std::min(std::abs(PreviousVelocityInDirection + 0.5 * AccelerationDeltaTime), RateLimit)
                    * DeltaTimeSeconds,
                AbsoluteDifference);

        PreviousVelocity = DifferenceSign
            * std::min(std::abs(PreviousVelocityInDirection + AccelerationDeltaTime), RateLimit);

        return State;
    }

    double TemporalSigmoidGetWithRateAccel(double& State, double& PreviousVelocity, double Sample,
        double DeltaTimeSeconds, double RateLimit, double StartAcceleration, double StopAcceleration)
    {
        if (DeltaTimeSeconds <= 0.0)
            return State;

        const double Difference = Sample - State;
        const double PreviousVelocityInDirection = PreviousVelocity
            * std::max(Sign(PreviousVelocity * Difference), 0.0);
        const double VelocitySquared = PreviousVelocityInDirection * PreviousVelocityInDirection;
        const double AbsoluteDifference = std::abs(Difference);
        const double DifferenceSign = Sign(Difference);

        double AccelerationDeltaTime = 0.0;
        const double TwiceAbsoluteDifference = AbsoluteDifference * 2.0;
        if (VelocitySquared > TwiceAbsoluteDifference * StopAcceleration && TwiceAbsoluteDifference > 0.0)
        {
            AccelerationDeltaTime = -DifferenceSign
                * std::min((VelocitySquared / TwiceAbsoluteDifference) * DeltaTimeSeconds,
                    std::abs(PreviousVelocityInDirection));
        }
        else
        {
            AccelerationDeltaTime = DifferenceSign * StartAcceleration * DeltaTimeSeconds;
        }

        State += DifferenceSign
            * std::min(std::min(std::abs(PreviousVelocityInDirection + 0.5 * AccelerationDeltaTime), RateLimit)
                    * DeltaTimeSeconds,
                AbsoluteDifference);

        PreviousVelocity = DifferenceSign
            * std::min(std::abs(PreviousVelocityInDirection + AccelerationDeltaTime), RateLimit);

        return State;
    }

    double VerticalToHorizontalFovDeg(double VerticalFovDeg, double AspectRatio)
    {
        const double Aspect = std::isfinite(AspectRatio) && AspectRatio > 0.5 && AspectRatio < 5.0
                ? AspectRatio : 16.0 / 9.0;

        // const double VerticalRad = std::clamp(VerticalFovDeg, 1.0, 170.0) * DegToRad;
        const double VerticalRad = VerticalFovDeg * DegToRad; // tmp disable clamping

        return 2.0 * std::atan(std::tan(VerticalRad * 0.5) * Aspect) * RadToDeg;
    }

    FMatrix3 RotatorToMatrix(const FRotator& Rotator)
    {
        const double Pitch = Rotator.Pitch * DegToRad;

        const double Yaw = Rotator.Yaw * DegToRad;

        const double Roll = Rotator.Roll * DegToRad;

        const double SinPitch = std::sin(Pitch);
        const double CosPitch = std::cos(Pitch);
        const double SinYaw = std::sin(Yaw);
        const double CosYaw = std::cos(Yaw);
        const double SinRoll = std::sin(Roll);
        const double CosRoll = std::cos(Roll);

        FMatrix3 Matrix{};

        Matrix.M[0][0] = CosPitch * CosYaw;
        Matrix.M[1][0] = CosPitch * SinYaw;
        Matrix.M[2][0] = SinPitch;

        Matrix.M[0][1] = SinRoll * SinPitch * CosYaw - CosRoll * SinYaw;
        Matrix.M[1][1] = SinRoll * SinPitch * SinYaw + CosRoll * CosYaw;
        Matrix.M[2][1] = -SinRoll * CosPitch;

        Matrix.M[0][2] = -(CosRoll * SinPitch * CosYaw + SinRoll * SinYaw);
        Matrix.M[1][2] = CosYaw * SinRoll - CosRoll * SinPitch * SinYaw;
        Matrix.M[2][2] = CosRoll * CosPitch;

        return Matrix;
    }

    FVector MatrixVehicleForward(const FMatrix3& Matrix)
    {
        return {
            Matrix.M[0][1], Matrix.M[1][1], Matrix.M[2][1]
        };
    }

    FVector MatrixVehicleUp(const FMatrix3& Matrix)
    {
        return {
            Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2]
        };
    }

    FVector Rotate(const FMatrix3& Matrix, const FVector& Vector)
    {
        return {
            Matrix.M[0][0] * Vector.X + Matrix.M[0][1] * Vector.Y + Matrix.M[0][2] * Vector.Z,

            Matrix.M[1][0] * Vector.X + Matrix.M[1][1] * Vector.Y + Matrix.M[1][2] * Vector.Z,

            Matrix.M[2][0] * Vector.X + Matrix.M[2][1] * Vector.Y + Matrix.M[2][2] * Vector.Z
        };
    }

    FRotator DirectionToRotator(const FVector& Direction)
    {
        const FVector NormalizedDirection = Normalized(Direction, FVector{1.0, 0.0, 0.0});

        const double Horizontal = std::sqrt(NormalizedDirection.X * NormalizedDirection.X + NormalizedDirection.Y * NormalizedDirection.Y);

        return {
            WrapDegrees(std::atan2(NormalizedDirection.Z, Horizontal) * RadToDeg), WrapDegrees(
                std::atan2(NormalizedDirection.Y, NormalizedDirection.X) * RadToDeg), 0.0
        };
    }
}
