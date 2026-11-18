// ITK: чтение DICOM-серии и обработка фильтрами -> VTK: 3D-визуализация.
// Запуск: itk_vtk <папка_DICOM> [стадия 0..5] [порог_кости_HU]
#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryShapeKeepNObjectsImageFilter.h>
#include <itkBinaryShapeOpeningImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkImageRegionIterator.h>
#include <itkImageToVTKImageFilter.h>
#include <itkMedianImageFilter.h>

#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkTextActor.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <cmath>
#include <iostream>
#include <iterator>
#include <string>

using CTImage = itk::Image<short, 3>;           // значения в единицах Хаунсфилда (HU)
using MaskImage = itk::Image<unsigned char, 3>;

constexpr char RibSeriesUID[] = "1.3.6.1.4.1.14519.5.2.1.6279.6001.179049373636438705059720603192";

// Полигон боковой стенки грудной клетки на аксиальных срезах этой серии.
// x — расстояние в пикселях от середины снимка, y — строка снимка.
bool IsInRibRegion(double x, double y)
{
  constexpr double outline[][2] = {
    {22, 96}, {85, 98}, {148, 124}, {185, 174}, {215, 248},
    {211, 306}, {162, 366}, {90, 399}, {39, 415}, {33, 356},
    {96, 340}, {138, 307}, {143, 247}, {136, 197}, {107, 157}, {22, 139}
  };
  bool inside = false;
  for (std::size_t i = 0, j = std::size(outline) - 1; i < std::size(outline); j = i++)
  {
    const auto& a = outline[i];
    const auto& b = outline[j];
    if ((a[1] > y) != (b[1] > y) && x < a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]))
    {
      inside = !inside;
    }
  }
  return inside;
}

MaskImage::Pointer MakeRibMask(const MaskImage* bones)
{
  const auto region = bones->GetLargestPossibleRegion();
  auto ribs = MaskImage::New();
  ribs->CopyInformation(bones);
  ribs->SetRegions(region);
  ribs->Allocate();
  ribs->FillBuffer(0);

  itk::ImageRegionIterator<MaskImage> voxel(ribs, region);
  for (; !voxel.IsAtEnd(); ++voxel)
  {
    const auto index = voxel.GetIndex();
    const auto offset = index - region.GetIndex();
    if (offset[2] >= 10 && offset[2] <= 117 && bones->GetPixel(index) != 0 &&
        IsInRibRegion(std::abs(offset[0] - 252.0), offset[1]))
    {
      voxel.Set(1);
    }
  }

  // Удаляем мелкие островки, сохраняя отдельные рёбра.
  auto cleanup = itk::BinaryShapeOpeningImageFilter<MaskImage>::New();
  cleanup->SetInput(ribs);
  cleanup->SetForegroundValue(1);
  cleanup->SetAttribute("NumberOfPixels");
  cleanup->SetLambda(150);
  cleanup->Update();
  MaskImage::Pointer result = cleanup->GetOutput();
  result->DisconnectPipeline();
  return result;
}

// DeepCopy: выход коннектора живёт, только пока жив сам коннектор.
template <class TImage>
vtkSmartPointer<vtkImageData> ToVtk(TImage* image)
{
  auto connector = itk::ImageToVTKImageFilter<TImage>::New();
  connector->SetInput(image);
  connector->Update();
  auto result = vtkSmartPointer<vtkImageData>::New();
  result->DeepCopy(connector->GetOutput());
  return result;
}

// Объёмный рендеринг КТ: передаточные функции заданы в HU (мягкие ткани полупрозрачны, кость ярче).
vtkSmartPointer<vtkVolume> MakeVolume(vtkImageData* data)
{
  auto mapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
  mapper->SetInputData(data);

  auto opacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
  opacity->AddPoint(-1000, 0.0);
  opacity->AddPoint(-100, 0.0);
  opacity->AddPoint(50, 0.05);
  opacity->AddPoint(300, 0.15);
  opacity->AddPoint(1200, 0.8);

  auto color = vtkSmartPointer<vtkColorTransferFunction>::New();
  color->AddRGBPoint(-1000, 0.0, 0.0, 0.0);
  color->AddRGBPoint(0, 0.75, 0.45, 0.35);
  color->AddRGBPoint(300, 1.0, 0.9, 0.8);
  color->AddRGBPoint(1200, 1.0, 1.0, 1.0);

  auto property = vtkSmartPointer<vtkVolumeProperty>::New();
  property->SetScalarOpacity(opacity);
  property->SetColor(color);
  property->SetInterpolationTypeToLinear();
  property->ShadeOn();

  auto volume = vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(mapper);
  volume->SetProperty(property);
  return volume;
}

vtkSmartPointer<vtkActor> MakeSurface(vtkImageData* mask)
{
  auto surface = vtkSmartPointer<vtkFlyingEdges3D>::New();
  surface->SetInputData(mask);
  surface->SetValue(0, 0.5);

  auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputConnection(surface->GetOutputPort());
  mapper->ScalarVisibilityOff();

  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetColor(0.95, 0.9, 0.8);
  return actor;
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <dicom_dir> [stage 0..5] [bone_threshold_HU=200]\n"
              << "  0 - original, 1 - median, 2 - threshold, 3 - closing, 4 - largest object, 5 - ribs\n";
    return EXIT_FAILURE;
  }
  const short boneThreshold = static_cast<short>(argc > 3 ? std::stoi(argv[3]) : 200);

  auto names = itk::GDCMSeriesFileNames::New();
  names->SetDirectory(argv[1]);
  const auto& seriesUIDs = names->GetSeriesUIDs();
  if (seriesUIDs.empty())
  {
    std::cerr << "No DICOM series in " << argv[1] << "\n";
    return EXIT_FAILURE;
  }
  const bool isRibSeries = seriesUIDs.front() == RibSeriesUID;
  const int stage = argc > 2 ? std::stoi(argv[2]) : (isRibSeries ? 5 : 4);
  if (stage == 5 && !isRibSeries)
  {
    std::cerr << "Stage 5 requires the bundled LIDC-IDRI CT series.\n";
    return EXIT_FAILURE;
  }

  auto reader = itk::ImageSeriesReader<CTImage>::New();
  reader->SetImageIO(itk::GDCMImageIO::New());
  reader->SetFileNames(names->GetFileNames(seriesUIDs.front()));

  // Каждая стадия обрабатывает результат предыдущей.
  // 1. Медианный фильтр 3x3x3: убирает шум, сохраняя границы.
  auto median = itk::MedianImageFilter<CTImage, CTImage>::New();
  median->SetInput(reader->GetOutput());
  median->SetRadius(1);

  // 2. Порог: кость = HU в диапазоне [boneThreshold, 3071].
  auto threshold = itk::BinaryThresholdImageFilter<CTImage, MaskImage>::New();
  threshold->SetInput(median->GetOutput());
  threshold->SetLowerThreshold(boneThreshold);
  threshold->SetUpperThreshold(3071);
  threshold->SetInsideValue(1);
  threshold->SetOutsideValue(0);

  // 3. Морфологическое закрытие шаром радиуса 1: заделывает мелкие дыры в кости.
  using Ball = itk::BinaryBallStructuringElement<unsigned char, 3>;
  Ball ball;
  ball.SetRadius(1);
  ball.CreateStructuringElement();
  auto closing = itk::BinaryMorphologicalClosingImageFilter<MaskImage, MaskImage, Ball>::New();
  closing->SetInput(threshold->GetOutput());
  closing->SetKernel(ball);
  closing->SetForegroundValue(1);

  // 4. Оставляем только самую большую связную компоненту (скелет без стола и шума).
  auto largest = itk::BinaryShapeKeepNObjectsImageFilter<MaskImage>::New();
  largest->SetInput(closing->GetOutput());
  largest->SetForegroundValue(1);
  largest->SetNumberOfObjects(1);
  largest->SetAttribute("NumberOfPixels");

  const char* stageNames[] = { "0: original (volume rendering)", "1: median filter (volume rendering)",
    "2: binary threshold (surface)", "3: morphological closing (surface)", "4: largest object (surface)",
    "5: ribs (surface)" };
  if (stage < 0 || stage > 5)
  {
    std::cerr << "Stage must be 0..5\n";
    return EXIT_FAILURE;
  }

  auto renderer = vtkSmartPointer<vtkRenderer>::New();
  try
  {
    if (stage <= 1)
    {
      CTImage* ct = stage == 0 ? reader->GetOutput() : median->GetOutput();
      ct->Update();
      renderer->AddVolume(MakeVolume(ToVtk(ct)));
    }
    else if (stage == 5)
    {
      closing->Update();
      const auto ribs = MakeRibMask(closing->GetOutput());
      renderer->AddActor(MakeSurface(ToVtk(ribs.GetPointer())));
    }
    else
    {
      MaskImage* mask = stage == 2 ? threshold->GetOutput()
        : stage == 3               ? closing->GetOutput()
                                   : largest->GetOutput();
      mask->Update();
      renderer->AddActor(MakeSurface(ToVtk(mask)));
    }
  }
  catch (const itk::ExceptionObject& e)
  {
    std::cerr << e << "\n";
    return EXIT_FAILURE;
  }

  const auto size = reader->GetOutput()->GetLargestPossibleRegion().GetSize();
  std::cout << "Volume: " << size[0] << "x" << size[1] << "x" << size[2] << ", stage " << stageNames[stage]
            << "\n";

  auto text = vtkSmartPointer<vtkTextActor>::New();
  text->SetInput(stageNames[stage]);
  text->SetPosition(10, 10);
  renderer->AddActor2D(text);
  renderer->SetBackground(0.1, 0.1, 0.15);
  renderer->ResetCamera();

  auto window = vtkSmartPointer<vtkRenderWindow>::New();
  window->AddRenderer(renderer);
  window->SetSize(900, 900);
  window->SetWindowName("ITK -> VTK");

  auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
  interactor->SetRenderWindow(window);
  window->Render();
  interactor->Start();
  return EXIT_SUCCESS;
}
