using Emgu.CV;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BibNumberDotNet
{
    public class TextDetectionSettings
    {
        private bool _isDarkOnLight = true;

        public bool IsDarkOnLight
        {
            get { return _isDarkOnLight; }
            set { _isDarkOnLight = value; }
        }

        private int _maxStrokeLength = 20;

        public int MaxStrokeLength
        {
            get { return _maxStrokeLength; }
            set { _maxStrokeLength = value; }
        }
        
        
    }

    public class Point
    {
        private int _x = 0;

        public int X
        {
            get { return _x; }
            set { _x = value; }
        }

        private int _y = 0;

        public int Y
        {
            get { return _y; }
            set { _y = value; }
        }

        private int _swt = 0;

        public int SWT
        {
            get { return _swt; }
            set { _swt = value; }
        }

        public Point(int x, int y)
        {
            _x = x;
            _y = y;
            _swt = 0;
        }
    }

    public struct Ray
    {
        private Point _p;

        public Point P
        {
            get { return _p; }
            set { _p = value; }
        }

        private Point _q;

        public Point Q
        {
            get { return _q; }
            set { _q = value; }
        }

        private List<Point> _points;

        public List<Point> Points
        {
            get
            { 
                if(_points == null)
                {
                    _points = new List<Point>();
                }

                return _points; 
            }
        }
        
        
        
    }

    class TextDetection
    {
        public static void StrokeWidthTransform(Mat input, Mat gradientX, Mat gradientY, TextDetectionSettings settings, out Mat swt, out List<Ray> rays)
        {
            var precision = 0.05;
            byte[,] strokeLengths = new byte[input.Rows, input.Cols];
            rays = new List<Ray>();

            for(int row = 0; row < input.Rows; row++)
            {
                for(int column = 0; column < input.Cols; column++)
                {
                    var currentValue = input.GetData(row, column)[0];

                    if(currentValue > 0)
                    {
                        Ray ray = new Ray();

                        Point p = new Point(column, row);
                        ray.P = p;
                        ray.Points.Add(p);

                        var nextX = column + 0.5;
                        var nextY = row + 0.5;
                        

                        var currentX = column;
                        var currentY = row;

                        double gradX;
                        double gradY;

                        ComputePixelGradients(gradientX, gradientY, p, out gradX, out gradY, settings);

                        while (true)
                        {
                            nextX += gradX * precision;
                            nextY += gradY * precision;

                            var floorNextX = Math.Floor(nextX);
                            var floorNextY = Math.Floor(nextY);

                            if(floorNextX != currentX
                                || floorNextY != currentY)
                            {
                                currentX = (int)floorNextX;
                                currentY = (int)floorNextY;

                                if(currentX < 0
                                    || currentX > input.Cols
                                    || currentY < 0
                                    || currentY > input.Rows)
                                {
                                    break;
                                }

                                Point newPoint = new Point(currentX, currentY);
                                ray.Points.Add(newPoint);

                                var newValue = input.GetData(currentY, currentX)[0];

                                if(newValue > 0)
                                {
                                    ray.Q = newPoint;

                                    double nextGradX;
                                    double nextGradY;

                                    ComputePixelGradients(gradientX, gradientY, newPoint, out nextGradX, out nextGradY, settings);

                                    var gradSum = gradX * -nextGradX + gradY * -nextGradY;
                                    var acos = Math.Acos(gradSum);

                                    if(acos > Math.PI / 2.0)
                                    {
                                        var xLength = ray.Q.X - ray.P.X;
                                        var yLength = ray.Q.Y - ray.P.Y;
                                        var length = (int)Math.Ceiling(Math.Sqrt(xLength * xLength - yLength * yLength));

                                        if(length > settings.MaxStrokeLength)
                                        {
                                            break;
                                        }

                                        foreach(var rayPoint in ray.Points)
                                        {
                                            var oldRayLength = strokeLengths[rayPoint.Y, rayPoint.X];

                                            if(oldRayLength == 0
                                                || length < oldRayLength)
                                            {
                                                strokeLengths[rayPoint.Y, rayPoint.X] = ToByte(length);
                                            }
                                        }

                                        rays.Add(ray);
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }

        public void MedianFilterSWT(byte[,] swt, List<Ray> rays)
        {
            foreach(var ray in rays)
            {
                foreach(var point in ray.Points)
                {
                    point.SWT = swt[point.Y, point.X];
                }

                ray.Points.Sort((l, r) => l.SWT.CompareTo(r.SWT));
                var medianSwt = ray.Points[ray.Points.Count / 2].SWT;

                foreach (var point in ray.Points)
                {
                    swt[point.Y, point.X] = ToByte(Math.Min(point.SWT, medianSwt));
                }
            }
        }

        public void FindConnectedComponents(byte[,] swt, List<Ray> rays, Mat image)
        {

        }

        private static byte ToByte(double d)
        {
            int round = (int)Math.Round(d);

            if (round > 255)
            {
                round = 255;
            }
            else if (round < 0)
            {
                round = 0;
            }

            return (byte)round;
        }

        private static void ComputePixelGradients(Mat gradientX, Mat gradientY, Point point, out double gradX, out double gradY, TextDetectionSettings settings)
        {
            gradX = gradientX.GetData(point.Y, point.X)[0];
            gradY = gradientY.GetData(point.Y, point.X)[0];

            var gradientMagnitude = Math.Sqrt(gradX * gradX + gradY * gradY);

            if (settings.IsDarkOnLight)
            {
                gradX = -gradX / gradientMagnitude;
                gradY = -gradY / gradientMagnitude;
            }
            else
            {
                gradX = gradX / gradientMagnitude;
                gradY = gradY / gradientMagnitude;
            }
        }
    }
}
