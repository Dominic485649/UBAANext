Get-ChildItem -Recurse -Include *.hpp,*.cpp -Path core,mocks,apps,tests | ForEach-Object {
    Write-Host "Formatting: $($_.FullName)"
    clang-format -i $_.FullName
}
