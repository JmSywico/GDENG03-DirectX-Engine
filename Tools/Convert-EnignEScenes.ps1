param(
    [string]$SourceRoot = 'D:\GitHub\source\repos\enignE',
    [string]$DestinationRoot = (Join-Path $PSScriptRoot '..\Scenes\enignE')
)

$ErrorActionPreference = 'Stop'
$destination = [IO.Path]::GetFullPath($DestinationRoot)
[IO.Directory]::CreateDirectory($destination) | Out-Null

function Write-Vec3($writer, $value) {
    $writer.Write([single]$value[0]); $writer.Write([single]$value[1]); $writer.Write([single]$value[2])
}
function Write-Vec4($writer, $value) {
    $writer.Write([single]$value[0]); $writer.Write([single]$value[1]); $writer.Write([single]$value[2]); $writer.Write([single]$value[3])
}
function Write-StringValue($writer, [string]$value) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($value)
    $writer.Write([uint32]$bytes.Length); $writer.Write($bytes)
}

function Convert-Scene([string]$sourcePath, [string]$outputName) {
    $document = Get-Content -Raw -LiteralPath $sourcePath | ConvertFrom-Json
    $supported = [Collections.Generic.List[object]]::new()
    $supportedIds = [Collections.Generic.HashSet[uint64]]::new()
    $warnings = [Collections.Generic.List[string]]::new()

    foreach ($entity in $document.entities) {
        $components = $entity.components
        $renderer = $components.MeshRendererComponent
        $light = $components.LightComponent
        if ($null -eq $renderer -and $null -eq $light) {
            $names = ($components.PSObject.Properties.Name -join ', ')
            $warnings.Add("Skipped '$($entity.name)' ($names)")
            continue
        }
        $supported.Add($entity)
        [void]$supportedIds.Add([uint64]$entity.id)
    }

    $nativePath = Join-Path $destination ($outputName + '.dx3dscene')
    $stream = [IO.File]::Open($nativePath, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::UTF8, $false)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DX3DSCNE'))
        $writer.Write([uint32]4)
        $writer.Write([uint32]$supported.Count)

        foreach ($entity in $supported) {
            $components = $entity.components
            $transform = $components.TransformComponent
            $renderer = $components.MeshRendererComponent
            $light = $components.LightComponent
            $rigid = $components.RigidBodyComponent
            $collider = $components.ColliderComponent

            $entityId = [uint64]$entity.id
            $parentId = [uint64]0
            if ($null -ne $entity.parent -and $supportedIds.Contains([uint64]$entity.parent)) { $parentId = [uint64]$entity.parent }
            $writer.Write($entityId); $writer.Write($parentId)

            $hasMaterial = $null -ne $renderer
            $writer.Write([uint32]$(if ($hasMaterial) { 1 } else { 0 }))
            if ($hasMaterial) {
                $mode = [math]::Min(4, [math]::Max(0, [int]$renderer.materialMode))
                $albedo = if ($null -ne $renderer.albedo) { $renderer.albedo } else { @(1,1,1,1) }
                $writer.Write([uint32]$mode); Write-Vec4 $writer $albedo
                Write-Vec3 $writer @(0,0,0); $writer.Write([single]0)
            }

            $hasPhysics = $null -ne $rigid -and $null -ne $collider
            $writer.Write([uint32]$(if ($hasPhysics) { 1 } else { 0 }))
            if ($hasPhysics) {
                $motion = [math]::Min(2, [math]::Max(0, [int]$rigid.motion))
                $shape = [math]::Min(1, [math]::Max(0, [int]$collider.shape))
                $writer.Write([uint32]$motion); $writer.Write([uint32]$shape)
                Write-Vec3 $writer $collider.halfExtents
                $writer.Write([single]$collider.radius)
                $writer.Write([single]$(if ($null -ne $rigid.mass) { $rigid.mass } else { 1.0 }))
                $writer.Write([single]$rigid.restitution)
                $writer.Write([uint32]$(if ([single]$rigid.gravityFactor -ne 0.0) { 1 } else { 0 }))
            }

            $position = @($transform.position)
            $rotation = @($transform.rotation)
            $scale = @([single]$transform.scale[0], [single]$transform.scale[1], [single]$transform.scale[2])
            $storedType = [uint32]4
            if ($null -ne $renderer) {
                $primitive = [string]$renderer.primitive.type
                switch ($primitive) {
                    'Plane' { $storedType = 2 }
                    'Grid' { $storedType = 2 }
                    'Rectangle' {
                        $storedType = 1
                        $scale[0] *= [single]$renderer.primitive.width
                        $scale[1] *= [single]$renderer.primitive.height
                        $scale[2] *= [single]$renderer.primitive.depth
                    }
                    'Sphere' {
                        $storedType = 1
                        $diameter = [single]$renderer.primitive.radius * 2.0
                        $scale[0] *= $diameter; $scale[1] *= $diameter; $scale[2] *= $diameter
                        $warnings.Add("Sphere render proxy: '$($entity.name)'")
                    }
                    default { $storedType = 1 }
                }
            }

            $writer.Write($storedType)
            Write-StringValue $writer ([string]$entity.name)
            Write-Vec3 $writer $position; Write-Vec3 $writer $rotation; Write-Vec3 $writer $scale

            if ($storedType -eq 4) {
                Write-Vec3 $writer $light.color
                $writer.Write([single]$light.intensity)
                $writer.Write([single]0.20)
                $writer.Write([single]$(if ($null -ne $light.shadowDistance) { $light.shadowDistance } else { 30.0 }))
                $writer.Write([uint32]$(if ($light.castShadows) { 1 } else { 0 }))
            }
        }
    }
    finally { $writer.Dispose() }

    $rawPath = Join-Path $destination ($outputName + '.escene')
    Copy-Item -LiteralPath $sourcePath -Destination $rawPath -Force
    $reportPath = Join-Path $destination ($outputName + '.import.txt')
    $report = @(
        "Source: $sourcePath",
        "Scene: $($document.scene.name)",
        "Source entities: $($document.entities.Count)",
        "Imported entities: $($supported.Count)",
        "Warnings: $($warnings.Count)"
    ) + $warnings
    [IO.File]::WriteAllLines($reportPath, $report)
}

Get-ChildItem -LiteralPath (Join-Path $SourceRoot 'scenes') -Filter '*.escene' | ForEach-Object {
    Convert-Scene $_.FullName $_.BaseName
}

$editorTest = Join-Path $SourceRoot 'enignE\scenes\test.escene'
if (Test-Path -LiteralPath $editorTest) { Convert-Scene $editorTest 'editor-test' }
